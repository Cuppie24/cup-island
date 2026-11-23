#pragma once
// A shiny edge on a round control (decision 165): on liquid glass, a filled
// button catches the light the way the island's own edge does, brightest at
// its top left, dim across the middle, bright again at the bottom right.

#include "core/tokens.h"
#include "platform/win.h"

#include <array>

namespace cup::platform {

struct Shine {
    wuc::CompositionSpriteShape shape{ nullptr }; // a stroke just inside the circle's edge
    std::array<wuc::CompositionColorGradientStop, 3> stops{ nullptr, nullptr, nullptr };
};

// For a circle of `radius` centred in `size`, `width` wide; clear until fadeShine().
Shine makeShine(wuc::Compositor const& c, Vec2 size, float radius, float width);

// To `opacity` at its brightest (0: none), over `duration`.
void fadeShine(const Shine& shine, float opacity, Ms duration);

} // namespace cup::platform
