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
    // On the page, the line being sung is the presenter's to say: it's lit and kept in view.
    auto line = lyricsPage_ && m.lyrics ? lyricIndexAt(Lyrics{ .timeline = m.timeline, .lines = m.lyrics }, host_.now()) : std::nullopt;
    m.lyricsLine = line ? static_cast<int>(*line) : -1;
}

void MediaPresenter::showVolume(const std::string& sessionKey, std::optional<float> volume, bool muted)
{
    volumeOf_ = sessionKey;
    volume_ = volume;
    muted_ = muted;
    if (last_ && !goneSince_ && last_->sessionKey == sessionKey) show(*last_);
}

// Open while the pointer is on the speaker or on the bar it opened into.
void MediaPresenter::hoverVolume(bool onBar, bool over)
{
    (onBar ? barHovered_ : speakerHovered_) = over;
    bool open = speakerHovered_ || barHovered_;
    if (open == volumeOpen_) return;
    volumeOpen_ = open;
    if (last_ && !goneSince_) show(*last_);
}

void MediaPresenter::closeVolume()
{
    speakerHovered_ = barHovered_ = false;
    if (!std::exchange(volumeOpen_, false)) return;
    if (last_ && !goneSince_) show(*last_);
}

void MediaPresenter::showLyricsPage(bool show)
{
    if (lyricsPage_ == show) return;
    lyricsPage_ = show;
    if (last_ && !goneSince_) this->show(*last_);
    armWake();
    if (show) syncLyricsPage(); // opened where the song is
}

void MediaPresenter::syncLyricsPage()
{
    if (lyricsPage_ && last_ && last_->media.lyrics) host_.reveal(kActivityId, media_action::kLyricsList, lyricsPageRow(last_->media));
}

// The host wakes a provider once (Host::wakeAt): for the earlier of a vanished session's
// grace and, on the lyrics' page, the next line's start.
void MediaPresenter::armWake()
{
    std::optional<Ms> at;
    if (goneSince_) at = *goneSince_ + tokens().mediaSessionGoneGrace;
    else if (lyricsPage_ && last_ && last_->media.lyrics)
        at = nextLyricChange(Lyrics{ .timeline = last_->media.timeline, .lines = last_->media.lyrics }, host_.now());
    host_.wakeAt(at);
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
    volumeOpen_ = speakerHovered_ = barHovered_ = false;
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
    goneSince_.reset();
    MediaSnapshot next = *snapshot;
    showing_ = true;
    fill(next);
    showing_ = false;
    if (last_ && last_->sessionKey == next.sessionKey && sameTrack(last_->media, next.media) &&
        sameTimeline(last_->media.timeline, next.media.timeline, now))
        next.media.timeline = last_->media.timeline;
    if (last_ && *last_ == next) {
        armWake(); // the same, but a wake may be owed (the page's next line)
        return;    // nothing new
    }

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
    armWake();
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
    if (goneSince_) {
        if (host_.now() - *goneSince_ >= tokens().mediaSessionGoneGrace) end();
        else armWake(); // woken early: asked again
        return;
    }
    // The lyrics' page: a new line is being sung. Shown lit, and the list moves on with it
    // unless the user has scrolled it away (Host::follow).
    if (lyricsPage_ && last_ && last_->media.lyrics) {
        show(*last_);
        host_.follow(kActivityId, media_action::kLyricsList, lyricsPageRow(last_->media));
    }
}

} // namespace cup
