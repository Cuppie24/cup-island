#include "indexer/query.h"

#include <algorithm>

namespace cup::indexer {
namespace {

char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string_view text)
{
    std::string out(text);
    for (char& c : out) c = lower(c);
    return out;
}

bool alnum(char c)
{
    c = lower(c);
    // Bytes of non-ASCII letters (UTF-8 lead and continuation bytes) count as
    // letters, so "café" isn't split inside its é.
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || static_cast<unsigned char>(c) >= 0x80;
}

// Where `lowerWord` first appears in `text` from `from`, ignoring ASCII case.
std::size_t findFolded(std::string_view text, std::string_view lowerWord, std::size_t from)
{
    if (lowerWord.size() > text.size()) return std::string_view::npos;
    for (std::size_t i = from; i + lowerWord.size() <= text.size(); ++i) {
        std::size_t j = 0;
        while (j < lowerWord.size() && lower(text[i + j]) == lowerWord[j]) ++j;
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

int matchScore(std::string_view name, std::string_view lowerWord)
{
    if (lowerWord.empty()) return 0;
    if (equalsFolded(name, lowerWord)) return 4;
    std::size_t at = findFolded(name, lowerWord, 0);
    if (at == std::string_view::npos) return 0;
    if (at == 0) return 3;
    for (; at != std::string_view::npos; at = findFolded(name, lowerWord, at + 1))
        if (!alnum(name[at - 1])) return 2;
    return 1;
}

std::vector<Hit> search(const FileIndex& index, const Query& query, std::size_t limit)
{
    std::vector<Hit> hits;
    if (query.empty() || limit == 0) return hits;
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
            int s = matchScore(name, word);
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
        hits.push_back({ record, score, folder, index.entry(record).nameLength });
    }
    std::size_t keep = std::min(limit, hits.size());
    std::partial_sort(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(keep), hits.end(), betterHit);
    hits.resize(keep);
    return hits;
}

bool betterHit(const Hit& a, const Hit& b)
{
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
