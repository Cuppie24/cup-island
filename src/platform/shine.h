#pragma once
// A shiny edge (decision 165): on liquid glass, the island's controls and
// the surfaces on it (a filled button, the skips, Home's widgets, a toast)
// catch the light as glass does: not all the way round, but on two corners,
// the top left and, fainter, the bottom right, a hairline fading out along
// the edge either side. (All the way round, and wider, it read as an
// outline drawn on, not as glass.)

#include "core/tokens.h"
#include "platform/win.h"

#include <array>

namespace cup::platform {

struct Shine {
    wuc::CompositionSpriteShape shape{ nullptr }; // a stroke just inside the edge: add it to a ShapeVisual's shapes
    wuc::CompositionRoundedRectangleGeometry outline{ nullptr };
    std::array<wuc::CompositionColorGradientStop, 4> stops{ nullptr, nullptr, nullptr, nullptr };
    float width = 0;
};

// One pixel wide at any scale (a hairline); clear, and nowhere, until placeShine() and fadeShine().
Shine makeShine(wuc::Compositor const& c);

// Just inside `area` (its ShapeVisual's coordinates), whose corners are
// `cornerRadius` round: half its side for a circle.
void placeShine(const Shine& shine, Rect area, float cornerRadius);

// To `opacity` at its brightest (0: none), over `duration`. Nothing if it was never made.
void fadeShine(const Shine& shine, float opacity, Ms duration);

} // namespace cup::platform
