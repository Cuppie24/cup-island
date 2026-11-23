#pragma once
// A wavy progress bar's wave (Progress::wavy, decision 167): a sine line over
// the bar's filled part, a path, so it's a shape like the bar's own.
//
// Three things move it, each on the compositor (TR §5):
// - Its head. The line hangs from its head (the path's x = 0), and its
//   shape's offset is the fill's live end: playback and a drag move it.
// - Its ripples. The path itself changes, from one phase of the wave to the
//   next (a path animation), and the swing of every phase shrinks to nothing
//   at the head, so the line straightens into it wherever the head is.
// - Which phase shows. Not the animation's own time: its progress is an
//   expression of a clock that loops once a ripple and of how far the head
//   has moved. So the ripples stay where they are on the bar while the head
//   is dragged through them, and only the straightening follows the head.
// Rising is the first stretch of the same animation (flat to the first
// phase); lying down is a path animation of its own, from the line as it is
// then to flat. Its start shows through a round-ended window.

#include "platform/win.h"

#include <chrono>
#include <d2d1_1.h>
#include <optional>
#include <vector>

namespace cup::platform {

struct WaveParts {
    wuc::ShapeVisual visual{ nullptr };        // over the bar, the frame's size; null: no wave (it couldn't be made)
    wuc::CompositionSpriteShape line{ nullptr };
    wuc::CompositionPathGeometry geometry{ nullptr };
    std::vector<wuc::CompositionPath> phases;  // the line at each step of one ripple
    wuc::CompositionPath flat{ nullptr };      // ...and lying flat
    // Rise 0..1: how far it has risen. Clock 0..1, looping: how far into a ripple. Anchor: where
    // the head was when it rose; the head's distance from there shifts the phase back, so the
    // ripples stay put on the bar.
    wuc::CompositionPropertySet state{ nullptr };
    wuc::CompositionRoundedRectangleGeometry fill{ nullptr }; // the bar's: its live end is the head
    ID2D1Factory* factory = nullptr;           // makes the one path lying down starts from
    Vec2 frame;
    std::optional<bool> rippling;              // what it was last told: a tick of the clock doesn't restart it
    // What the compositor is showing can't be read back (decision 001): kept here to work out
    // the line as it is when it's told to lie down.
    std::chrono::steady_clock::time_point roseAt{};
    Ms rise{};          // how long that rise takes; the clock starts when it ends
    bool runs = false;  // the clock runs (not with reduced motion)
    float anchor = 0;
};

// `frame`: the bar's, the wave's line through its middle. `accent`: the
// bar's brush, so a recolour fades both. `fill`: the bar's filled part,
// whose live end is the wave's head. Flat and still until ripple().
WaveParts createWave(wuc::Compositor const& c, ID2D1Factory* factory, Vec2 frame, wuc::CompositionColorBrush const& accent,
                     wuc::CompositionRoundedRectangleGeometry const& fill);
// Playing: it rises and its ripples run to its head; not: it straightens into a flat line and
// stops. `head`: the fill's width now. `atOnce`: with no rising or lying down, as it's made.
void ripple(wuc::Compositor const& c, WaveParts& wave, bool playing, float head, bool atOnce);

} // namespace cup::platform
