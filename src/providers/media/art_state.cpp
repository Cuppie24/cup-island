#include "providers/media/art_state.h"

#include "core/tokens.h"

namespace cup {
namespace {

bool samePixels(const ImageData* a, const ImageData* b)
{
    if (!a || !b) return a == b;
    return a->width == b->width && a->height == b->height && a->bgra == b->bgra;
}

} // namespace

std::optional<std::uint64_t> ArtState::onRead(const std::string& track, bool hasThumbnail, bool propertiesChanged, Ms now)
{
    if (track != track_) {
        // A new track: the old cover is wrong for it. Hold the track back
        // briefly so it (and its alert) appear with the right picture.
        track_ = track;
        art_.reset();
        pending_.reset();
        holdUntil_.reset();
        if (!hasThumbnail) return std::nullopt;
        holdUntil_ = now + tokens().mediaArtWait;
        return pending_ = nextId_++;
    }
    if (!propertiesChanged) return std::nullopt;
    if (!hasThumbnail) { // the app dropped its picture
        art_.reset();
        pending_.reset();
        holdUntil_.reset();
        return std::nullopt;
    }
    // Same track, re-sent properties: the picture may have changed (Chrome).
    // Keep showing the current one meanwhile.
    return pending_ = nextId_++;
}

bool ArtState::onDecoded(std::uint64_t id, std::shared_ptr<const ImageData> image)
{
    if (pending_ != id) return false; // a newer decode or track took over
    pending_.reset();
    bool wasHolding = holdUntil_.has_value();
    holdUntil_.reset();
    // A failed re-decode keeps the picture we have; a failed first decode shows none.
    if (!image && art_) return wasHolding;
    bool changed = !samePixels(image.get(), art_.get());
    if (changed) art_ = std::move(image);
    return changed || wasHolding;
}

bool ArtState::holding(Ms now) const
{
    return holdUntil_ && now < *holdUntil_;
}

} // namespace cup
