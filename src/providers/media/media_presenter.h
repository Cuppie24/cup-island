#pragma once
// Turns successive "what's playing" snapshots into Host calls: when to show,
// update, alert and end. No WinRT here, so every rule is tested on Linux;
// the SMTC provider only reads Windows and feeds snapshots in.

#include "core/provider.h"
#include "providers/lyrics/lyrics_lookup.h"
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
    // `lyrics`: how to ask for a song's lyrics (decision 180); none: no lyrics line.
    explicit MediaPresenter(Host& host, LyricsLookup::Fetch lyrics = {});

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
    // The volume of a session's app, as just read or set (decision 180);
    // nullopt: it has none to show. Kept for as long as that session is shown.
    void showVolume(const std::string& sessionKey, std::optional<float> volume, bool muted);
    // The pointer came onto or left the card's speaker (`onBar` false) or the volume's bar it
    // opens into: open while it's on either (decision 180).
    void hoverVolume(bool onBar, bool over);
    void closeVolume(); // the card closed: shut, whatever the pointer was on
    void onWake();      // the grace is over: end what's shown, unless a session came back
    void showLyricsPage(bool show); // the line being sung clicked, or the page's back arrow; the card closing goes back too
    void toggleLyrics(); // the card's lyrics button: off, nothing is looked up and no line shown; remembered

    static constexpr const char* kLyricsKey = "lyrics"; // Host::save: "off", or anything else for on
    static constexpr const char* kActivityId = "media.nowPlaying"; // also its panel's (decision 009)

private:
    void end();
    void fill(MediaSnapshot& snapshot); // its lyrics and volume, which the reader doesn't know

    Host& host_;
    std::optional<MediaSnapshot> last_;
    std::optional<Ms> goneSince_; // no session since then; what's shown waits out the grace
    bool hasLyrics_ = false; // a lyrics service was given (before lyrics_: set from the same argument)
    LyricsLookup lyrics_;
    bool lyricsOn_ = true;   // the lyrics button; kept under kLyricsKey
    bool lyricsShown_ = false; // the last song whose lookup ended had lyrics: the next keeps the line's row meanwhile
    bool showing_ = false;     // in show(): a lookup answered at once is already in what's being shown
    std::string volumeOf_;     // the session whose volume this is
    std::optional<float> volume_;
    bool muted_ = false;
    bool volumeOpen_ = false;
    bool speakerHovered_ = false, barHovered_ = false;
    bool lyricsPage_ = false; // the card is the lyrics' page while the song has lyrics
};

} // namespace cup
