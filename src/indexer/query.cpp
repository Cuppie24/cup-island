#include "indexer/query.h"

#include <algorithm>
#include <optional>

namespace cup::indexer {
namespace {

char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// The byte at `k` with English and Russian letters folded to lower case.
// Russian's А-Я (U+0410-042F) are D0 90..D0 AF in UTF-8; а-п are D0 B0..BF and
// р-я D1 80..8F. Each capital folds to a small letter of the same length, so
// matching stays byte by byte: only the lead byte of Р-Я changes (D0 -> D1)
// and the second byte moves by 0x20. Ё/ё and other Cyrillic letters (і, ї, ў...)
// match only as typed (decision 075). 0xD0 is never a continuation byte, so
// looking one byte back is safe.
char foldedAt(std::string_view text, std::size_t k)
{
    auto b = static_cast<unsigned char>(text[k]);
    if (b < 0x80) return lower(text[k]);
    if (b == 0xD0) {
        bool capitalRtoYa = k + 1 < text.size() && static_cast<unsigned char>(text[k + 1]) >= 0xA0 &&
                            static_cast<unsigned char>(text[k + 1]) <= 0xAF;
        return capitalRtoYa ? static_cast<char>(0xD1) : text[k];
    }
    if (b >= 0x90 && b <= 0xAF && k > 0 && static_cast<unsigned char>(text[k - 1]) == 0xD0)
        return static_cast<char>(b <= 0x9F ? b + 0x20 : b - 0x20);
    return text[k];
}

std::string lowered(std::string_view text)
{
    std::string out(text);
    for (std::size_t k = 0; k < text.size(); ++k) out[k] = foldedAt(text, k);
    return out;
}

bool alnum(char c)
{
    c = lower(c);
    // Bytes of non-ASCII letters (UTF-8 lead and continuation bytes) count as
    // letters, so "café" isn't split inside its é.
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || static_cast<unsigned char>(c) >= 0x80;
}

// Where `lowerWord` first appears in `text` from `from`, ignoring English and Russian case.
std::size_t findFolded(std::string_view text, std::string_view lowerWord, std::size_t from)
{
    if (lowerWord.empty() || lowerWord.size() > text.size()) return std::string_view::npos;
    // The raw bytes that can fold to the word's first byte: at most two, so
    // most positions are skipped with a plain comparison instead of folding.
    char first = lowerWord[0], other = first;
    if (first >= 'a' && first <= 'z') other = static_cast<char>(first - 'a' + 'A');
    else if (static_cast<unsigned char>(first) == 0xD1) other = static_cast<char>(0xD0); // р-я from Р-Я
    for (std::size_t i = from; i + lowerWord.size() <= text.size(); ++i) {
        if (text[i] != first && text[i] != other) continue;
        std::size_t j = 0;
        while (j < lowerWord.size() && foldedAt(text, i + j) == lowerWord[j]) ++j;
        if (j == lowerWord.size()) return i;
    }
    return std::string_view::npos;
}

// Splits on spaces; a quoted run is one token (the quotes dropped), and a
// filter's value can be quoted too: path:"my music".
std::vector<std::string> tokens(std::string_view text)
{
    std::vector<std::string> out;
    std::string current;
    bool quoted = false, any = false;
    for (char c : text) {
        if (c == '"') {
            quoted = !quoted;
            any = true;
        } else if (c == ' ' && !quoted) {
            if (any) out.push_back(current);
            current.clear();
            any = false;
        } else {
            current += c;
            any = true;
        }
    }
    if (any) out.push_back(current);
    return out;
}

void addTerm(std::vector<std::string>& to, std::string term, std::size_t& terms)
{
    if (term.empty() || terms >= kMaxTerms) return;
    to.push_back(std::move(term));
    ++terms;
}

std::string_view extensionOf(std::string_view name)
{
    std::size_t dot = name.rfind('.');
    if (dot == std::string_view::npos || dot == 0) return {};
    return name.substr(dot + 1);
}

bool equalsFolded(std::string_view text, std::string_view lowerWord)
{
    return text.size() == lowerWord.size() && findFolded(text, lowerWord, 0) == 0;
}

// Whether each folder is in or is a noise folder, worked out once per
// folder per search: hits share folders, so walking every hit's whole
// chain again was most of a search's time.
class NoiseMemo {
public:
    explicit NoiseMemo(const FileIndex& index) : index_(index), known_(index.slots(), kUnknown) {}

    bool inNoise(std::uint32_t record) // any folder above `record`?
    {
        std::uint32_t parent = index_.entry(record).parent;
        return parent != record && below(parent);
    }

private:
    static constexpr std::uint8_t kUnknown = 0, kNo = 1, kYes = 2;

    // `folder` is noise or inside noise. Walks up to the first known answer,
    // then writes it to every folder passed on the way.
    bool below(std::uint32_t folder)
    {
        passed_.clear();
        std::uint8_t answer = kNo;
        std::uint32_t at = folder;
        for (std::size_t depth = 0; depth < 1024; ++depth) {
            if (at == kRootRecord || !index_.used(at)) break;
            if (known_[at] != kUnknown) {
                answer = known_[at];
                break;
            }
            passed_.push_back(at);
            if (index_.entry(at).flags & kNoise) {
                answer = kYes;
                break;
            }
            std::uint32_t parent = index_.entry(at).parent;
            if (parent == at) break;
            at = parent;
        }
        for (std::uint32_t f : passed_) known_[f] = answer;
        return answer == kYes;
    }

    const FileIndex& index_;
    std::vector<std::uint8_t> known_;
    std::vector<std::uint32_t> passed_; // reused: one walk at a time
};

// UTF-8 characters: every byte but continuation bytes (10xxxxxx) starts one.
std::uint16_t characters(std::string_view name)
{
    std::uint16_t count = 0;
    for (char c : name)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
    return count;
}

} // namespace

Query parseQuery(std::string_view text)
{
    Query q;
    std::size_t terms = 0;
    for (std::string& token : tokens(text.substr(0, kMaxQueryLength))) {
        std::string t = lowered(token);
        if (t.starts_with("ext:")) {
            std::string value = t.substr(4);
            std::replace(value.begin(), value.end(), ',', ';');
            std::size_t start = 0;
            while (start <= value.size()) {
                std::size_t end = std::min(value.find(';', start), value.size());
                std::string ext = value.substr(start, end - start);
                if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
                addTerm(q.extensions, ext, terms);
                start = end + 1;
            }
        } else if (t.starts_with("path:")) {
            addTerm(q.pathWords, t.substr(5), terms);
        } else if (t == "file:") {
            q.kind = Kind::Files;
        } else if (t.starts_with("folder:")) {
            q.kind = Kind::Folders;
            addTerm(q.words, t.substr(7), terms);
        } else {
            addTerm(q.words, t, terms);
        }
    }
    return q;
}

int matchScore(std::string_view name, std::string_view lowerWord, bool folder)
{
    if (lowerWord.empty()) return 0;
    std::size_t at = findFolded(name, lowerWord, 0);
    if (at == std::string_view::npos) return 0;
    if (at == 0) {
        // A whole-name match starts at 0 too: only names that get here pay
        // for the extension.
        if (name.size() == lowerWord.size()) return 4;
        std::string_view ext = folder ? std::string_view{} : extensionOf(name);
        return !ext.empty() && name.size() - ext.size() - 1 == lowerWord.size() ? 4 : 3;
    }
    for (; at != std::string_view::npos; at = findFolded(name, lowerWord, at + 1))
        if (!alnum(name[at - 1])) return 2;
    return 1;
}

std::vector<Hit> search(const FileIndex& index, const Query& query, std::size_t limit)
{
    std::vector<Hit> hits;
    if (query.empty() || limit == 0) return hits;
    bool demoteNoise = std::none_of(query.pathWords.begin(), query.pathWords.end(),
                                    [](const std::string& w) { return noiseFolderName(w); });
    for (std::uint32_t record = 0; record < index.slots(); ++record) {
        if (!index.used(record)) continue;
        bool folder = index.entry(record).flags & kFolder;
        if ((query.kind == Kind::Files && folder) || (query.kind == Kind::Folders && !folder)) continue;
        std::string_view name = index.name(record);
        if (!query.extensions.empty()) {
            std::string_view ext = folder ? std::string_view{} : extensionOf(name);
            bool any = std::any_of(query.extensions.begin(), query.extensions.end(),
                                   [&](const std::string& e) { return equalsFolded(ext, e); });
            if (!any) continue;
        }
        int score = 0;
        bool all = true;
        for (const std::string& word : query.words) {
            int s = matchScore(name, word, folder);
            if (s == 0) {
                all = false;
                break;
            }
            score += s;
        }
        if (!all) continue;
        // Paths are built last, and only for names that already match.
        if (!query.pathWords.empty()) {
            std::uint32_t parent = index.entry(record).parent;
            std::string folderPath = index.used(parent) && parent != record ? index.path(parent, "") : std::string{};
            bool inPath = std::all_of(query.pathWords.begin(), query.pathWords.end(), [&](const std::string& w) {
                return findFolded(folderPath, w, 0) != std::string_view::npos;
            });
            if (!inPath) continue;
        }
        hits.push_back({ record, score, folder, characters(name), false });
    }
    if (!demoteNoise) {
        std::size_t keep = std::min(limit, hits.size());
        std::partial_sort(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(keep), hits.end(), betterHit);
        hits.resize(keep);
        return hits;
    }
    // Noise is the first thing betterHit compares, but finding it walks up
    // folders, which is slow for every hit of a common word. So: order on
    // the rest, then look for noise only in that order until `limit` hits
    // outside it are found. Only a head of the list is sorted at first; the
    // rest only if noise used the head up.
    std::size_t head = std::min(hits.size(), limit * 4 + 16);
    std::partial_sort(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(head), hits.end(), betterHit);
    NoiseMemo noise(index);
    std::vector<Hit> clean, noisy;
    for (std::size_t i = 0; i < hits.size() && clean.size() < limit; ++i) {
        if (i == head) std::sort(hits.begin() + static_cast<std::ptrdiff_t>(head), hits.end(), betterHit);
        Hit& h = hits[i];
        h.noise = noise.inNoise(h.record);
        (h.noise ? noisy : clean).push_back(h);
    }
    for (std::size_t i = 0; clean.size() < limit && i < noisy.size(); ++i) clean.push_back(noisy[i]);
    return clean;
}

bool betterHit(const Hit& a, const Hit& b)
{
    if (a.noise != b.noise) return b.noise;
    if (a.score != b.score) return a.score > b.score;
    if (a.folder != b.folder) return a.folder;
    if (a.nameLength != b.nameLength) return a.nameLength < b.nameLength;
    return a.record < b.record;
}

std::vector<Pick> mergeHits(const std::vector<std::vector<Hit>>& lists, std::size_t limit)
{
    std::vector<Pick> all;
    for (std::size_t l = 0; l < lists.size(); ++l)
        for (std::size_t i = 0; i < lists[l].size(); ++i) all.push_back({ l, i });
    auto better = [&](const Pick& a, const Pick& b) {
        const Hit& ha = lists[a.list][a.index];
        const Hit& hb = lists[b.list][b.index];
        if (betterHit(ha, hb)) return true;
        if (betterHit(hb, ha)) return false;
        return a.list < b.list;
    };
    // Each list is already sorted, so this is a merge; a sort keeps it short.
    std::stable_sort(all.begin(), all.end(), better);
    if (all.size() > limit) all.resize(limit);
    return all;
}

} // namespace cup::indexer
