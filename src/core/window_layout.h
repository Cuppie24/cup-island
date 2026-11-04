#pragma once
// A floating window's scene (decision 081): our own title bar, with the
// card's label and a ✕, over the card laid out as Expanded lays it out, at
// the window's size. Window coordinates, logical px. Pure.

#include "core/floating.h"
#include "core/layout.h"

namespace cup {

// The ✕'s actionId: the app closes the window (WindowClosed); no provider sees it.
inline constexpr const char* kCloseWindow = "#close-window";

Scene windowScene(const IslandState& state, const FloatingWindow& window, Ms now, const MeasureText& measure = {});

} // namespace cup
