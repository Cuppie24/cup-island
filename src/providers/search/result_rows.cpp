#include "providers/search/result_rows.h"

#include "indexer/query.h"

#include <algorithm>
#include <cctype>

namespace cup {
namespace {

bool wordStart(const std::string& folded, std::size_t at)
{
    if (at == 0) return true;
    auto c = static_cast<unsigned char>(folded[at - 1]);
    return c < 0x80 && !std::isalnum(c); // after a space, '-', '_', '.'...
}

} // namespace

std::vector<std::pair<std::size_t, std::size_t>> matchedParts(const std::string& name, const std::string& query)
{
    indexer::Query q = indexer::parseQuery(query);
    std::vector<std::string> words = q.words;
    for (const indexer::PathWord& p : q.paths) words.push_back(p.parts.back());
    std::string folded = indexer::foldCase(name); // same byte length: offsets hold (query.h)
    std::vector<std::pair<std::size_t, std::size_t>> out;
    for (const std::string& word : words) {
        if (word.empty()) continue;
        std::size_t best = std::string::npos;
        for (std::size_t at = folded.find(word); at != std::string::npos; at = folded.find(word, at + 1)) {
            if (best == std::string::npos) best = at;
            if (wordStart(folded, at)) { best = at; break; }
        }
        if (best != std::string::npos) out.emplace_back(best, word.size());
    }
    // In order, overlaps merged: "re rep" lights "rep" once.
    std::sort(out.begin(), out.end());
    std::vector<std::pair<std::size_t, std::size_t>> merged;
    for (auto part : out) {
        if (!merged.empty() && part.first <= merged.back().first + merged.back().second)
            merged.back().second = std::max(merged.back().second, part.first + part.second - merged.back().first);
        else merged.push_back(part);
    }
    return merged;
}

std::string shortPath(const std::string& path, const std::string& home)
{
    if (home.empty() || path.size() < home.size()) return path;
    if (indexer::foldCase(path.substr(0, home.size())) != indexer::foldCase(home)) return path;
    if (path.size() > home.size() && path[home.size()] != '\\') return path; // C:\Users\Said2 isn't inside
    return "~" + path.substr(home.size());
}

std::string folderOf(const std::string& path)
{
    std::size_t slash = path.find_last_of('\\');
    if (slash == std::string::npos) return path;
    if (slash == 2 && path.size() > 3) return path.substr(0, 3); // a drive's top level: "C:\"
    return path.substr(0, slash);
}

} // namespace cup
