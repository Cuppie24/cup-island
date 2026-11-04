#pragma once
// A floating card's scene (decision 081): a title bar strip holding its ✕,
// like a window's, over the card laid out as Expanded lays it out, every
// corner round (floatCornerRadius). Card coordinates, logical px. Pure.

#include "core/floating.h"
#include "core/layout.h"

namespace cup {

// The ✕'s actionId: the app sends the card back (FloatClosed); no provider sees it.
inline constexpr const char* kCloseFloat = "#close-float";
inline constexpr const char* kCloseFloatKey = "float/close"; // its element

// Its size: as resized by the user, else its width class's width and as tall
// as its content needs plus the title bar (a stacked card's rows; a Standard
// card is as tall as Expanded's), within its limits (floatLimits).
Scene floatScene(const IslandState& state, const FloatingCard& card, Ms now, const MeasureText& measure = {});

} // namespace cup
