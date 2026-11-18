#include "providers/search/result_rows.h"

#include "indexer/query.h"

#include <algorithm>
#include <cctype>
#include <string_view>

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

std::string expandShortcuts(const std::string& query, const std::string& home,
                            const std::function<std::string(const std::string& name)>& variable)
{
    if (query.find_first_of("~%") == std::string::npos) return query;
    std::string out;
    bool quoted = false;
    // What the shortcut at `at` stands for, and how long it is; nothing if
    // it isn't one, or isn't known.
    auto shortcutAt = [&](std::size_t at, std::size_t& length) -> std::string {
        if (at >= query.size()) return {};
        if (query[at] == '~') {
            length = 1;
            return home;
        }
        if (query[at] != '%' || !variable) return {};
        std::size_t close = query.find('%', at + 1);
        if (close == std::string::npos || close == at + 1) return {};
        std::string name = query.substr(at + 1, close - at - 1);
        for (char c : name) // a variable's name: PROGRAMFILES(X86) too, never a path or a space
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '(' && c != ')') return {};
        length = close - at + 1;
        return variable(name);
    };
    for (std::size_t i = 0; i < query.size();) {
        bool wordStart = !quoted && query[i] != ' ' && (i == 0 || query[i - 1] == ' ');
        if (wordStart) {
            // Copy its opening quote and a path: filter, then look for a
            // shortcut (indexer/query.cpp's tokenizer splits the same way).
            auto quote = [&] {
                if (i < query.size() && query[i] == '"') {
                    quoted = !quoted;
                    out += query[i++];
                }
            };
            quote();
            if (indexer::foldCase(query.substr(i, 5)) == "path:") {
                out += query.substr(i, 5);
                i += 5;
            }
            quote();
            std::size_t length = 0;
            std::string value = shortcutAt(i, length);
            std::size_t after = i + length;
            bool ends = after >= query.size() || std::string_view("/\\\" ").find(query[after]) != std::string_view::npos;
            if (!value.empty() && ends) {
                bool spaced = value.find(' ') != std::string::npos;
                out += spaced && !quoted ? '"' + value + '"' : value;
                i = after;
            }
            if (i >= query.size()) break;
        }
        if (query[i] == '"') quoted = !quoted;
        out += query[i++];
    }
    return out;
}

std::string folderOf(const std::string& path)
{
    std::size_t slash = path.find_last_of('\\');
    if (slash == std::string::npos) return path;
    if (slash == 2 && path.size() > 3) return path.substr(0, 3); // a drive's top level: "C:\"
    return path.substr(0, slash);
}

} // namespace cup
