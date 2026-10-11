#pragma once
// Turns successive "what's playing" snapshots into Host calls: when to show,
// update, alert and end. No WinRT here, so every rule is tested on Linux;
// the SMTC provider only reads Windows and feeds snapshots in.

#include "core/provider.h"
#include "providers/media/now_playing.h"

#include <optional>
#include <string>

namespace cup {

struct MediaSnapshot {
    std::string sessionKey; // which app's session (its AUMID)
    NowPlaying media;
    bool operator==(const MediaSnapshot&) const = default;
};

class MediaPresenter {
public:
    explicit MediaPresenter(Host& host) : host_(host) {}

    void start(); // publishes the media widget (decision 019), there even when nothing plays
    void stop();  // ends what's shown, removes the widget

    // nullopt = no media session at all.
    void show(const std::optional<MediaSnapshot>& snapshot);

    static constexpr const char* kActivityId = "media.nowPlaying"; // also the widget's panel (decision 009)

private:
    Host& host_;
    std::optional<MediaSnapshot> last_;
    std::optional<Ms> pausedSince_;
};

} // namespace cup
