#pragma once
// Which browser tab is playing the media, from tab names alone: SMTC says
// "Chrome is playing <title>" but not which tab (decision 005). Pure, so
// the guess is tested on Linux; app_activation reads the tab names.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// The tab whose name contains the media title (any case): "Song - YouTube"
// for "Song". Among several, one marked as playing audio wins, then the
// first. Nothing for a title too short to mean anything, or no match.
std::optional<std::size_t> findMediaTab(const std::vector<std::string>& tabNames, const std::string& mediaTitle);

} // namespace cup
