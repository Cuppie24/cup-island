#include "providers/media/tab_match.h"

#include <algorithm>
#include <cctype>

namespace cup {
namespace {

// ASCII-only case folding: enough for "YouTube" vs "youtube"; other scripts compare as written.
std::string fold(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// What Chromium browsers (Chrome, Edge…) append to a tab's accessible name
// while it makes sound, in English UIs. Elsewhere it's only a missing tiebreak.
bool marksAudio(const std::string& foldedName)
{
    return foldedName.find("audio playing") != std::string::npos || foldedName.find("playing audio") != std::string::npos;
}

} // namespace

std::optional<std::size_t> findMediaTab(const std::vector<std::string>& tabNames, const std::string& mediaTitle)
{
    constexpr std::size_t kMinTitle = 2;
    std::string title = fold(mediaTitle);
    if (title.size() < kMinTitle) return std::nullopt;

    std::optional<std::size_t> best;
    bool bestPlays = false;
    for (std::size_t i = 0; i < tabNames.size(); ++i) {
        std::string name = fold(tabNames[i]);
        if (name.find(title) == std::string::npos) continue;
        bool plays = marksAudio(name);
        if (!best || (plays && !bestPlays)) {
            best = i;
            bestPlays = plays;
        }
    }
    return best;
}

} // namespace cup
