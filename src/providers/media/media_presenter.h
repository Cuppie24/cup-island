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

    void start(); // publishes media's panel: its page-row icon and card, there even when nothing plays (decision 165)
    void stop();  // ends what's shown, removes the panel

    // nullopt = no media session at all. What's shown stays for
    // mediaSessionGoneGrace first (decision 074): some apps (Yandex Music)
    // drop their session for a moment on every track change.
    void show(const std::optional<MediaSnapshot>& snapshot);
    // Only the playback of what's shown changed (play/pause, the timeline,
    // shuffle, repeat, which controls work): `playback`'s fields go over the
    // last snapshot's, title and cover kept. Nothing if another session (or
    // none) is shown: the full read brings that (decision 116).
    void showPlayback(const std::string& sessionKey, const NowPlaying& playback);
    void onWake(); // the grace is over: end what's shown, unless a session came back

    static constexpr const char* kActivityId = "media.nowPlaying"; // also its panel's (decision 009)

private:
    void end();

    Host& host_;
    std::optional<MediaSnapshot> last_;
    std::optional<Ms> goneSince_; // no session since then; what's shown waits out the grace
};

} // namespace cup
