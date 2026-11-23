#pragma once
// A wavy progress bar's wave (Progress::wavy, decision 167): a sine line over
// the bar's filled part, drawn as a path so it's a shape like the bar's own.
// It's cut where the fill ends (a clip that reads the fill's live width), so
// it follows playback and scrubbing with no code of its own; it ripples
// back from its head while playing; when not, it sinks away and the bar's
// own straight fill shows instead.
// Neither end is a straight cut: its start shows through a round-ended
// window, and its head has a round cap that rides it (createProgress). All on the
// compositor (TR §5).

#include "platform/win.h"

#include <d2d1_1.h>
#include <optional>

namespace cup::platform {

struct WaveParts {
    wuc::ContainerVisual visual{ nullptr };    // over the bar, the frame's size; null: no wave (it couldn't be made)
    wuc::CompositionSpriteShape line{ nullptr };
    // The bar's own straight fill: what's shown when the wave lies flat (set by createProgress).
    wuc::CompositionSpriteShape flat{ nullptr };
    // Riding: 1 while the knob rides the wave's head, 0 while it's dragged: then it's on the bar's line.
    wuc::CompositionPropertySet knob{ nullptr };
    std::optional<bool> rippling;              // what it was last told: a tick of the clock doesn't restart it
};

// `frame`: the bar's, the wave's line through its middle. `accent`: the
// bar's brush, so a recolour fades both. `fill`: the bar's filled part,
// whose width cuts the wave. Flat and still until ripple().
WaveParts createWave(wuc::Compositor const& c, ID2D1Factory* factory, Vec2 frame, wuc::CompositionColorBrush const& accent,
                     wuc::CompositionRoundedRectangleGeometry const& fill);
// Playing: it rises and runs along; not: it sinks into the bar's straight fill and stops.
// `atOnce`: with no settling, as it's made.
void ripple(wuc::Compositor const& c, WaveParts& wave, bool playing, bool atOnce);
// The knob on the wave's head (hover), or on the bar's line (dragged).
void rideHead(wuc::Compositor const& c, const WaveParts& wave, bool riding);

} // namespace cup::platform
