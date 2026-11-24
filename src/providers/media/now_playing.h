#pragma once
// The island's "now playing" content (PS §10 Media row), shared by the real
// media provider and the demo one so they always look the same.

#include "core/content.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

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
    bool canOpenSource = false;    // clicking the expanded art brings the playing app forward
    // The song's words by time (decision 180); null: none known. The card
    // shows the line being sung over its bar.
    std::shared_ptr<const std::vector<LyricLine>> lyrics;
    // Being looked up, after a song that had some: the line's row stays,
    // empty, so the bar under it doesn't jump at every new song.
    bool lyricsPending = false;
    // Whether lyrics are looked up and shown, as the card's lyrics button has
    // it; none: there's no lyrics service, and no button.
    std::optional<bool> lyricsOn;
    // On, and this song has none (looked up, or not a song to look up): the button is disabled.
    bool lyricsMissing = false;
    // The playing app's own volume, 0..1, as Windows' mixer has it (decision
    // 180); none: not known (the app has no audio session), and no control.
    std::optional<float> volume;
    bool muted = false;
    bool volumeOpen = false; // its speaker button was clicked: the volume's bar is shown, floating above it

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
inline constexpr const char* kOpenSource = "openSource"; // the expanded art was clicked
inline constexpr const char* kSeek = "seek"; // value = 0..1 fraction
inline constexpr const char* kVolume = "volume"; // value = 0..1: the app's volume, sent as its bar is dragged
inline constexpr const char* kVolumeOpen = "volumeOpen"; // toggle: the volume's bar shown, or not
inline constexpr const char* kLyrics = "lyrics"; // toggle: lyrics looked up and shown, or not
} // namespace media_action

Content nowPlayingContent(const NowPlaying& media);

// Media's card while nothing plays: its icon is always in the page row
// (decision 165), so there's one place to look for what's playing.
Content nothingPlayingContent();

// "Track 3 of 12", "Track 3", or "" when the app doesn't say. It's the
// album position (all SMTC reports), not a place in the play queue.
std::string trackPosition(int number, int count);

// The repeat mode a click on the repeat button asks for.
Repeat nextRepeat(Repeat current);

} // namespace cup
