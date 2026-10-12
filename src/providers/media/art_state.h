#pragma once
// When to decode a media session's cover and whether to wait for it before
// showing the track (decision 005). Pure, so the rules are tested on Linux;
// the SMTC provider does the decoding and the timers.
//
// Two app habits drive it. Chrome reports every tab through one session and
// sends a new title and its picture in separate updates, so the cover is
// re-decoded on every properties update, not only when the title changes.
// And a thumbnail can fail to load at all, so a new track waits for its
// cover at most mediaArtWait, then shows without it.

#include "core/content.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace cup {

class ArtState {
public:
    // A read of the session. `track`: app|title|artist|album. `propertiesChanged`:
    // the app re-sent its media properties since the last read (the picture may
    // be new even if the track isn't). Returns the id of a decode to start.
    std::optional<std::uint64_t> onRead(const std::string& track, bool hasThumbnail, bool propertiesChanged, Ms now);

    // A decode finished (`image` null = it failed). True if what to show
    // changed, so the session should be read again.
    bool onDecoded(std::uint64_t id, std::shared_ptr<const ImageData> image);

    // A new track's cover is still decoding and its wait hasn't run out:
    // hold the track back, so its alert shows with the picture.
    bool holding(Ms now) const;
    std::optional<Ms> holdUntil() const { return holdUntil_; } // when to show it anyway

    const std::shared_ptr<const ImageData>& art() const { return art_; }

private:
    std::string track_;
    std::shared_ptr<const ImageData> art_;
    std::uint64_t nextId_ = 1;
    std::optional<std::uint64_t> pending_; // only the latest decode counts
    std::optional<Ms> holdUntil_;
};

} // namespace cup
