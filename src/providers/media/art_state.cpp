#include "providers/media/art_state.h"

namespace cup {
namespace {

bool samePixels(const ImageData* a, const ImageData* b)
{
    if (!a || !b) return a == b;
    return a->width == b->width && a->height == b->height && a->bgra == b->bgra;
}

} // namespace

std::optional<std::uint64_t> ArtState::onRead(const std::string& track, bool hasThumbnail, bool propertiesChanged)
{
    if (track != track_) {
        // A new track: the old cover is wrong for it. The track shows at once;
        // its cover fades in once read.
        track_ = track;
        art_.reset();
        pending_.reset();
        if (!hasThumbnail) return std::nullopt;
        return pending_ = nextId_++;
    }
    if (!propertiesChanged) return std::nullopt;
    if (!hasThumbnail) { // the app dropped its picture
        art_.reset();
        pending_.reset();
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
    // A failed re-decode keeps the picture we have; a failed first decode shows none.
    if (!image) return false;
    bool changed = !samePixels(image.get(), art_.get());
    if (changed) art_ = std::move(image);
    return changed;
}

} // namespace cup
