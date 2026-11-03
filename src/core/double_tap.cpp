#include "core/double_tap.h"

#include "core/tokens.h"

namespace cup {

bool DoubleTap::feed(int key, bool down, std::uint32_t atMs)
{
    if (key != key_) {
        // Another key, even while ours is held (Ctrl+C): no tap of ours.
        if (down) {
            inTap_ = false;
            firstDone_ = false;
            second_ = false;
        }
        return false;
    }
    if (down) {
        if (held_) return false; // a held key's repeat
        held_ = true;
        inTap_ = true;
        downAt_ = atMs;
        // Unsigned differences: right across the hook clock's wrap too.
        second_ = firstDone_ && atMs - firstUpAt_ <= static_cast<std::uint32_t>(tokens().doubleTap.count());
        firstDone_ = false;
        return false;
    }
    held_ = false;
    // Up: a tap if it was short and clean; a long hold is no tap.
    bool tap = inTap_ && atMs - downAt_ <= static_cast<std::uint32_t>(tokens().doubleTapHold.count());
    inTap_ = false;
    if (second_ && tap) {
        second_ = false; // a third tap starts a new pair
        return true;
    }
    second_ = false;
    firstDone_ = tap;
    firstUpAt_ = atMs;
    return false;
}

} // namespace cup
