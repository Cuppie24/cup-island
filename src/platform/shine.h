#pragma once
// A shiny edge (decision 165): on liquid glass, the island's controls and
// the surfaces on it (a filled button, the skips, Home's widgets, a toast)
// catch the light the way the island's own edge does: brightest at the top
// left, dim across the middle, bright again at the bottom right.

#include "core/tokens.h"
#include "platform/win.h"

#include <array>

namespace cup::platform {

struct Shine {
    wuc::CompositionSpriteShape shape{ nullptr }; // a stroke just inside the edge: add it to a ShapeVisual's shapes
    wuc::CompositionRoundedRectangleGeometry outline{ nullptr };
    std::array<wuc::CompositionColorGradientStop, 3> stops{ nullptr, nullptr, nullptr };
    float width = 0;
};

// `width` wide; clear, and nowhere, until placeShine() and fadeShine().
Shine makeShine(wuc::Compositor const& c, float width);

// Just inside `area` (its ShapeVisual's coordinates), whose corners are
// `cornerRadius` round: half its side for a circle.
void placeShine(const Shine& shine, Rect area, float cornerRadius);

// To `opacity` at its brightest (0: none), over `duration`. Nothing if it was never made.
void fadeShine(const Shine& shine, float opacity, Ms duration);

} // namespace cup::platform
