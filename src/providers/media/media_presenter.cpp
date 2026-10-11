#include "providers/media/media_presenter.h"

#include "core/tokens.h"

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

void MediaPresenter::show(const std::optional<MediaSnapshot>& snapshot)
{
    if (!snapshot) {
        if (last_) host_.endActivity(kActivityId);
        last_.reset();
        pausedSince_.reset();
        return;
    }
    Ms now = host_.now();
    MediaSnapshot next = *snapshot;
    if (last_ && last_->sessionKey == next.sessionKey && sameTrack(last_->media, next.media) &&
        sameTimeline(last_->media.timeline, next.media.timeline, now))
        next.media.timeline = last_->media.timeline;
    if (last_ && *last_ == next) return; // nothing new

    // Paused media lingers, then ends by itself (core expiry). The clock starts
    // at the first paused snapshot; later ones (seeks, art) don't extend it.
    if (next.media.timeline.playing) pausedSince_.reset();
    else if (!pausedSince_) pausedSince_ = now;

    // Alert only for a new track in the same app: not on startup, not when
    // an app appears, not when switching apps with the source button.
    bool newTrack = last_ && last_->sessionKey == next.sessionKey && !sameTrack(last_->media, next.media) &&
                    !next.media.title.empty();

    Content content = nowPlayingContent(next.media);
    Activity activity{ .id = kActivityId, .relevance = 0.6f, .content = content };
    if (pausedSince_) activity.expiresAt = *pausedSince_ + tokens().mediaPausedLinger;
    host_.startActivity(activity);
    if (newTrack) host_.postAlert({ .activityId = kActivityId, .content = content, .coalesceKey = "track" });

    last_ = next;
}

} // namespace cup
