#pragma once

#include "core/geometry.h"

namespace cup {

// CSS-style cubic-bezier easing from (0,0) to (1,1) with control points c1, c2.
// Returns eased progress for t in [0, 1]. Same curve the compositor runs, so
// app-side code can follow an animation it can't read back (decision 001).
float cubicBezierEase(Vec2 c1, Vec2 c2, float t);

} // namespace cup
