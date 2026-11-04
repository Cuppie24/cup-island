#pragma once
// A floating card's scene (decision 081): the card laid out as Expanded lays
// it out, in the island's look with every corner round, and a ✕ on its
// top-right corner. Card coordinates, logical px. Pure.

#include "core/floating.h"
#include "core/layout.h"

namespace cup {

// The ✕'s actionId: the app sends the card back (FloatClosed); no provider sees it.
inline constexpr const char* kCloseFloat = "#close-float";
inline constexpr const char* kCloseFloatKey = "float/close"; // its element: shown while the pointer is on the card

// Its size: as resized by the user, else its width class's width and as tall
// as its content needs (a stacked card's rows, up to floatMaxHeight; a
// Standard card is as tall as Expanded's).
Scene floatScene(const IslandState& state, const FloatingCard& card, Ms now, const MeasureText& measure = {});

} // namespace cup
