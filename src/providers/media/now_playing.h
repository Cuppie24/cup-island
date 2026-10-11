#pragma once
// The island's "now playing" content (PS §10 Media row), shared by the real
// media provider and the demo one so they always look the same.

#include "core/content.h"

#include <memory>
#include <string>

namespace cup {

struct NowPlaying {
    std::string title;
    std::string artist;
    std::string source; // app name, e.g. "Spotify"; may be empty
    std::shared_ptr<const ImageData> art;
    Timeline timeline;  // duration 0 = the app reports no timeline
    bool canPrevious = true;
    bool canPlayPause = true;
    bool canNext = true;
    bool canSeek = false;
    bool showSourceSwitch = false; // more than one media app to choose from

    bool operator==(const NowPlaying&) const = default;
};

// Action ids a now-playing activity sends back to its provider.
namespace media_action {
inline constexpr const char* kPrevious = "previous";
inline constexpr const char* kPlayPause = "playPause";
inline constexpr const char* kNext = "next";
inline constexpr const char* kSource = "source";
inline constexpr const char* kSeek = "seek"; // value = 0..1 fraction
} // namespace media_action

Content nowPlayingContent(const NowPlaying& media);

} // namespace cup
