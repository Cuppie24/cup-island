#include "providers/media/media_presenter.h"

#include "core/tokens.h"

#include <utility>

namespace cup {
namespace {

bool sameTrack(const NowPlaying& a, const NowPlaying& b) { return a.title == b.title && a.artist == b.artist; }

// Apps re-send timelines often (some every second), each with a slightly
// different anchor. If the new one agrees with where the old one says we are,
// keep the old one: no republish, no progress bar re-arm, no jitter.
bool sameTimeline(const Timeline& a, const Timeline& b, Ms now)
{
    constexpr Ms kTolerance{ 750 };
    Ms drift = a.positionAt(now) - b.positionAt(now);
    return a.playing == b.playing && a.duration == b.duration && a.rate == b.rate && drift < kTolerance && drift > -kTolerance;
}

} // namespace

MediaPresenter::MediaPresenter(Host& host, LyricsLookup::Fetch lyrics)
    : host_(host), hasLyrics_(static_cast<bool>(lyrics)), lyrics_(std::move(lyrics), [this] {
          // A song's lyrics came (or there are none): shown as it is now, with them.
          if (last_ && !goneSince_ && !showing_) show(*last_);
      })
{
}

void MediaPresenter::fill(MediaSnapshot& snapshot)
{
    NowPlaying& m = snapshot.media;
    m.lyricsOn = hasLyrics_ ? std::optional<bool>(lyricsOn_) : std::nullopt;
    // Off: nothing is asked of the service, so no song's name leaves the PC.
    LyricsAnswer answer = lyricsOn_ ? lyrics_.lyricsFor({ m.title, m.artist, m.timeline.duration }) : LyricsAnswer{};
    if (answer.state != LyricsAnswer::State::Looking) lyricsShown_ = answer.state == LyricsAnswer::State::Found;
    m.lyrics = answer.lines;
    m.lyricsPending = answer.state == LyricsAnswer::State::Looking && lyricsShown_;
    m.lyricsMissing = lyricsOn_ && answer.state == LyricsAnswer::State::None;
    bool known = snapshot.sessionKey == volumeOf_;
    m.volume = known ? volume_ : std::nullopt;
    m.muted = known && muted_;
    m.volumeOpen = volumeOpen_;
    m.lyricsPage = lyricsPage_;
}

void MediaPresenter::showVolume(const std::string& sessionKey, std::optional<float> volume, bool muted)
{
    volumeOf_ = sessionKey;
    volume_ = volume;
    muted_ = muted;
    if (last_ && !goneSince_ && last_->sessionKey == sessionKey) show(*last_);
}

void MediaPresenter::toggleVolume()
{
    volumeOpen_ = !volumeOpen_;
    volumeHideAt_ = volumeOpen_ ? std::optional<Ms>(host_.now() + tokens().volumeBarTimeout) : std::nullopt;
    armWake();
    if (last_ && !goneSince_) show(*last_);
}

void MediaPresenter::touchVolume()
{
    if (!volumeOpen_) return;
    volumeHideAt_ = host_.now() + tokens().volumeBarTimeout;
    armWake();
}

// The host wakes a provider once (Host::wakeAt): for the earlier of the session's grace and the volume bar's timeout.
void MediaPresenter::armWake()
{
    std::optional<Ms> at = volumeHideAt_;
    if (goneSince_) {
        Ms grace = *goneSince_ + tokens().mediaSessionGoneGrace;
        if (!at || grace < *at) at = grace;
    }
    host_.wakeAt(at);
}

void MediaPresenter::closeVolume()
{
    if (volumeOpen_) toggleVolume();
}

void MediaPresenter::showLyricsPage(bool show)
{
    if (lyricsPage_ == show) return;
    lyricsPage_ = show;
    if (last_ && !goneSince_) this->show(*last_);
}

void MediaPresenter::toggleLyrics()
{
    lyricsOn_ = !lyricsOn_;
    host_.save(kLyricsKey, lyricsOn_ ? "on" : "off");
    if (last_ && !goneSince_) show(*last_);
}

void MediaPresenter::start()
{
    lyricsOn_ = host_.load(kLyricsKey) != "off";
    host_.publishPanel({ .id = kActivityId, .content = nothingPlayingContent(), .icon = Glyph::Music });
}

void MediaPresenter::stop()
{
    end();
    host_.removePanel(kActivityId);
}

void MediaPresenter::end()
{
    if (last_) host_.endActivity(kActivityId);
    last_.reset();
    volumeOpen_ = false;
    volumeHideAt_.reset();
    goneSince_.reset();
    host_.wakeAt(std::nullopt);
}

void MediaPresenter::show(const std::optional<MediaSnapshot>& snapshot)
{
    Ms now = host_.now();
    if (!snapshot) {
        // Not ended at once: a session that's back within the grace was a
        // track change (decision 074). The clock starts at the first gap.
        if (last_ && !goneSince_) {
            goneSince_ = now;
            armWake();
        }
        return;
    }
    if (std::exchange(goneSince_, std::nullopt)) armWake();
    MediaSnapshot next = *snapshot;
    showing_ = true;
    fill(next);
    showing_ = false;
    if (last_ && last_->sessionKey == next.sessionKey && sameTrack(last_->media, next.media) &&
        sameTimeline(last_->media.timeline, next.media.timeline, now))
        next.media.timeline = last_->media.timeline;
    if (last_ && *last_ == next) return; // nothing new

    // Alert only for a new track in the same app: not on startup, not when
    // an app appears, not when switching apps with the source button.
    bool newTrack = last_ && last_->sessionKey == next.sessionKey && !sameTrack(last_->media, next.media) &&
                    !next.media.title.empty();

    Content content = nowPlayingContent(next.media);
    // No expiry, paused or not: it stays as long as Windows has the session (decision 099).
    host_.startActivity({ .id = kActivityId, .relevance = 0.6f, .content = content });
    if (newTrack) host_.postAlert({ .activityId = kActivityId, .content = content, .coalesceKey = "track" });
    // The track shows before its cover is read (decision 056): its alert, if
    // still up or queued, gets the cover too.
    else if (last_ && last_->sessionKey == next.sessionKey && sameTrack(last_->media, next.media) && last_->media.art != next.media.art)
        host_.postAlert({ .activityId = kActivityId, .content = content, .coalesceKey = "track", .refreshOnly = true });

    last_ = next;
}

void MediaPresenter::showPlayback(const std::string& sessionKey, const NowPlaying& playback)
{
    if (!last_ || last_->sessionKey != sessionKey || goneSince_) return;
    MediaSnapshot next = *last_;
    NowPlaying& m = next.media;
    m.timeline = playback.timeline;
    m.canPrevious = playback.canPrevious;
    m.canPlayPause = playback.canPlayPause;
    m.canNext = playback.canNext;
    m.canSeek = playback.canSeek;
    m.canShuffle = playback.canShuffle;
    m.shuffle = playback.shuffle;
    m.canRepeat = playback.canRepeat;
    m.repeat = playback.repeat;
    show(next);
}

void MediaPresenter::onWake()
{
    Ms now = host_.now();
    if (goneSince_ && now - *goneSince_ >= tokens().mediaSessionGoneGrace) {
        end();
        return;
    }
    if (volumeHideAt_ && now >= *volumeHideAt_) closeVolume(); // left alone: put away
    else armWake();                                            // the other time is still to come
}

} // namespace cup
