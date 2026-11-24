#pragma once
// Recently opened files rank higher in search (decision 075's plan, built in
// decision 082). The island keeps what was opened from its search, per user
// (Host::save), and works it into the service's answer itself: the service
// never learns what anyone opens, and the protocol doesn't change.
//   - within the same match class only: a recent file moves up past files
//     that match no better, never above a better match;
//   - the more recent (and the more often), the higher; older than a month
//     counts for nothing (LaunchHistory::weight);
//   - a recent file the service's answer left out still shows, when the
//     query is plain words that all match its name;
//   - one that's gone from disk is dropped from the list.
// Pure, tested on Linux.

#include "indexer/protocol.h"
#include "providers/search/launchables.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

// The history's key for a file result: its path, a folder's with a
// trailing '\' (so it shows as a folder when the service didn't send it).
std::string recentKey(const indexer::ResultRow& row);
indexer::ResultRow rowOfKey(const std::string& key); // its name, path and folder back

// How well `query` matches `row`'s name, as the service's match classes
// (indexer::matchScore, a path word by its last part); 0 for a word that
// isn't in it.
int fileMatch(const indexer::ResultRow& row, std::string_view query);

// The service's `rows` with what `opened` remembers worked in, at most
// `limit`. `exists` says whether a remembered path is still there; the gone
// ones are added to `gone`, for the caller to forget.
std::vector<indexer::ResultRow> withRecent(std::vector<indexer::ResultRow> rows, const LaunchHistory& opened,
                                           std::string_view query, Ms now, std::size_t limit,
                                           const std::function<bool(const std::string& path)>& exists,
                                           std::vector<std::string>& gone);

} // namespace cup
