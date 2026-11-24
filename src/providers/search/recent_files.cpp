#include "providers/search/recent_files.h"

#include "indexer/query.h"

#include <algorithm>

namespace cup {

std::string recentKey(const indexer::ResultRow& row) { return row.folder ? row.path + "\\" : row.path; }

indexer::ResultRow rowOfKey(const std::string& key)
{
    bool folder = !key.empty() && key.back() == '\\';
    std::string path = folder ? key.substr(0, key.size() - 1) : key;
    std::size_t slash = path.find_last_of('\\');
    return { .name = slash == std::string::npos ? path : path.substr(slash + 1), .path = path, .folder = folder };
}

int fileMatch(const indexer::ResultRow& row, std::string_view query)
{
    indexer::Query q = indexer::parseQuery(query);
    int total = 0;
    for (const std::string& w : q.words) {
        int s = indexer::matchScore(row.name, w, row.folder);
        if (s == 0) return 0;
        total += s;
    }
    for (const indexer::PathWord& p : q.paths) total += indexer::matchScore(row.name, p.parts.back(), row.folder);
    return total;
}

std::vector<indexer::ResultRow> withRecent(std::vector<indexer::ResultRow> rows, const LaunchHistory& opened, std::string_view query,
                                           Ms now, std::size_t limit, const std::function<bool(const std::string&)>& exists,
                                           std::vector<std::string>& gone)
{
    indexer::Query q = indexer::parseQuery(query);
    // Only plain words can be matched here without the index: with a filter
    // (ext:, path:, a path word, a drive) a recent file shows only if the
    // service sent it, and is just ranked.
    bool plain = !q.words.empty() && q.paths.empty() && q.extensions.empty() && q.pathWords.empty() && !q.drive && q.kind == indexer::Kind::Any;
    if (plain) {
        for (const auto& [key, use] : opened.uses) {
            if (opened.weight(key, now) <= 0) continue;
            indexer::ResultRow row = rowOfKey(key);
            bool sent = std::any_of(rows.begin(), rows.end(), [&](const indexer::ResultRow& r) { return r.path == row.path; });
            if (sent || fileMatch(row, query) == 0) continue;
            if (exists && !exists(row.path)) {
                gone.push_back(key);
                continue;
            }
            rows.push_back(row); // after the service's: placed by the pass below
        }
    }
    // Each row's class and how recently it was opened; then recent rows move
    // up past rows that match no better and were opened less recently. The
    // service's order otherwise stands (folders, depth, noise: query.h).
    struct Ranked {
        indexer::ResultRow row;
        int match;
        double recent;
    };
    std::vector<Ranked> ranked;
    for (indexer::ResultRow& r : rows) {
        int match = fileMatch(r, query);
        double recent = opened.weight(recentKey(r), now);
        ranked.push_back({ std::move(r), match, recent });
    }
    for (std::size_t i = 1; i < ranked.size(); ++i)
        for (std::size_t j = i; j > 0 && ranked[j].recent > ranked[j - 1].recent && ranked[j].match >= ranked[j - 1].match; --j)
            std::swap(ranked[j], ranked[j - 1]);
    std::vector<indexer::ResultRow> out;
    for (std::size_t i = 0; i < ranked.size() && out.size() < limit; ++i) out.push_back(std::move(ranked[i].row));
    return out;
}

} // namespace cup
