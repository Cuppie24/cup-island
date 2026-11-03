#pragma once
// A modifier pressed twice quickly, on its own: "Double Ctrl", search's
// shortcut (decision 079). Fed every key going down or up, system-wide, with
// its time; says when the second tap is done. Pure, so it's tested here; the
// platform's low-level keyboard hook feeds it.
//
// A tap is the key going down and up again within doubleTapHoldMs, with no
// other key in between; the second must go down within doubleTapMs of the
// first going up. It fires as the second tap lets go, not as it goes down:
// "Ctrl, Ctrl+C" typed fast is no double tap (the C breaks the second), and
// nothing opens while Ctrl is still held, which would make the first letter
// typed a Ctrl shortcut. Anything else pressed in between starts over.
// Held keys repeat their "down": repeats are ignored.

#include <cstdint>

namespace cup {

class DoubleTap {
public:
    // `key`: kCtrl, kShift or kAlt (core/shortcuts.h). Either side's key
    // counts: the app passes left and right Ctrl as kCtrl.
    explicit DoubleTap(int key) : key_(key) {}

    // A key event; true when it completes a double tap. Times in ms, any
    // origin (the hook's own clock), only differences matter.
    bool feed(int key, bool down, std::uint32_t atMs);

private:
    int key_;
    bool held_ = false;          // the key is down now
    bool inTap_ = false;         // it went down cleanly: no other key since
    std::uint32_t downAt_ = 0;
    bool firstDone_ = false;     // one clean tap finished...
    std::uint32_t firstUpAt_ = 0; // ...at this time
    bool second_ = false;        // the key went down in time to be the second tap
};

} // namespace cup
