#pragma once
// The island's "now playing" content (PS §10 Media row), shared by the real
// media provider and the demo one so they always look the same.

#include "core/content.h"

#include <memory>
#include <string>

namespace cup {

enum class Repeat { Off, All, One };

struct NowPlaying {
    std::string title;
    std::string artist;
    std::string source; // app name, e.g. "Spotify"; may be empty
    int trackNumber = 0; // position on the album, as the app reports it; 0 = unknown
    int trackCount = 0;  // tracks on the album; 0 = unknown
    std::shared_ptr<const ImageData> art;
    Timeline timeline;  // duration 0 = the app reports no timeline
    bool canPrevious = true;
    bool canPlayPause = true;
    bool canNext = true;
    bool canSeek = false;
    bool canShuffle = false; // the app reports and accepts shuffle; otherwise the button is hidden
    bool shuffle = false;
    bool canRepeat = false;
    Repeat repeat = Repeat::Off;
    bool showSourceSwitch = false; // more than one media app to choose from

    bool operator==(const NowPlaying&) const = default;
};

// Action ids a now-playing activity sends back to its provider.
namespace media_action {
inline constexpr const char* kPrevious = "previous";
inline constexpr const char* kPlayPause = "playPause";
inline constexpr const char* kNext = "next";
inline constexpr const char* kShuffle = "shuffle"; // toggle
inline constexpr const char* kRepeat = "repeat";   // cycle: off -> all -> one -> off
inline constexpr const char* kSource = "source";
inline constexpr const char* kSeek = "seek"; // value = 0..1 fraction
} // namespace media_action

Content nowPlayingContent(const NowPlaying& media);

// "Track 3 of 12", "Track 3", or "" when the app doesn't say. It's the
// album position (all SMTC reports), not a place in the play queue.
std::string trackPosition(int number, int count);

// The repeat mode a click on the repeat button asks for.
Repeat nextRepeat(Repeat current);

} // namespace cup
