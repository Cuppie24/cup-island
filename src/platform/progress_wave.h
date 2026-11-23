#pragma once
// A wavy progress bar's wave (Progress::wavy, decision 167): a sine line over
// the bar's filled part, a path, so it's a shape like the bar's own.
// The line hangs from its head: its shape's offset is the fill's live end,
// so it follows playback and scrubbing with no code of its own. Its ripples
// are the path itself changing, from one phase of the wave to the next
// (a path animation), which is what lets it straighten into its head and
// lie truly flat, at its full thickness, when nothing plays. Its start
// shows through a round-ended window. All on the compositor (TR §5).

#include "platform/win.h"

#include <d2d1_1.h>
#include <memory>
#include <optional>
#include <vector>

namespace cup::platform {

struct WaveParts {
    wuc::ShapeVisual visual{ nullptr };        // over the bar, the frame's size; null: no wave (it couldn't be made)
    wuc::CompositionSpriteShape line{ nullptr };
    wuc::CompositionPathGeometry geometry{ nullptr };
    std::vector<wuc::CompositionPath> phases;  // the line at each step of one ripple
    wuc::CompositionPath flat{ nullptr };      // ...and lying flat
    std::optional<bool> rippling;              // what it was last told: a tick of the clock doesn't restart it
    // Counts ripple()s: a rise that ends after another one was asked for doesn't start the run.
    std::shared_ptr<int> turn;
};

// `frame`: the bar's, the wave's line through its middle. `accent`: the
// bar's brush, so a recolour fades both. `fill`: the bar's filled part,
// whose live end is the wave's head. Flat and still until ripple().
WaveParts createWave(wuc::Compositor const& c, ID2D1Factory* factory, Vec2 frame, wuc::CompositionColorBrush const& accent,
                     wuc::CompositionRoundedRectangleGeometry const& fill);
// Playing: it rises and its ripples run to its head; not: it lies flat and stops.
// `atOnce`: with no settling, as it's made.
void ripple(wuc::Compositor const& c, WaveParts& wave, bool playing, bool atOnce);

} // namespace cup::platform
