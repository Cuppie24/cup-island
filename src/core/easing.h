#pragma once

#include "core/tokens.h"

namespace cup {

// Eased progress for t in [0, 1] along a CSS-style cubic-bezier curve. Same curve the compositor runs, so
// app-side code can follow an animation it can't read back (decision 001).
float cubicBezierEase(Bezier curve, float t);

} // namespace cup
