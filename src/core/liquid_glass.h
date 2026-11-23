#pragma once
// Liquid glass (decision 165): what the shader that bends the desktop needs
// to know about the island's shapes. The outline on screen is cut exactly by
// the compositor; this only says where the rim's bend goes, so a pixel or
// two of difference mid-morph doesn't show. Pure: time is passed in.

#include "core/geometry.h"
#include "core/motion.h"
#include "core/tokens.h"

namespace cup {

// One shape as the shader sees it: a rounded rectangle, render-layer pixels.
struct LiquidShape {
    Rect frame;
    float radius = 0;
    bool operator==(const LiquidShape&) const = default;
};

// `body` (a pill's or a bubble's bounds) as a shape: the island's corner
// radius, and while it hangs from the screen edge (`attached`), its top
// carried far enough past that edge that no rim bends along it: only the
// sides and the bottom are a pane's lip. Nothing (an empty frame) if it has no size.
LiquidShape liquidShape(Rect body, bool attached, const Tokens& t);

// Where a morph from `from` to `to` has got to, `elapsed` after it started:
// the same spring the compositor runs (springProgress), overshoot included.
Rect morphRect(Rect from, Rect to, const Motion& motion, Ms elapsed);

// The pixels a frame must draw for `shapes`: around them all, `margin` wider
// (their ears, their shadow's edge). Empty if there's nothing to draw.
Rect liquidBounds(const std::vector<LiquidShape>& shapes, float margin);

} // namespace cup
