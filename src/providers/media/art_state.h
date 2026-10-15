#pragma once
// When to decode a media session's cover (decisions 005, 056). Pure, so the rules are tested on Linux;
// the SMTC provider does the decoding and the timers.
//
// Two app habits drive it. Chrome reports every tab through one session and
// sends a new title and its picture in separate updates, so the cover is
// re-decoded on every properties update, not only when the title changes.
// A new track shows at once, without a cover; the cover fades in when it's
// read (a thumbnail can be slow, or never load).

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
    // A new track drops the old cover at once: it's wrong for it.
    std::optional<std::uint64_t> onRead(const std::string& track, bool hasThumbnail, bool propertiesChanged);

    // A decode finished (`image` null = it failed). True if the cover
    // changed, so the session should be read again.
    bool onDecoded(std::uint64_t id, std::shared_ptr<const ImageData> image);

    const std::shared_ptr<const ImageData>& art() const { return art_; }

private:
    std::string track_;
    std::shared_ptr<const ImageData> art_;
    std::uint64_t nextId_ = 1;
    std::optional<std::uint64_t> pending_; // only the latest decode counts
};

} // namespace cup
