#pragma once
// The island's context menu over a card (decision 066): its items stacked in
// a panel where it was asked for, kept inside the card. Part of layout();
// split out of layout.cpp to keep it readable.

#include "core/layout.h"

namespace cup {

// Where the menu goes: at `at`, flipped left or up where it would leave `card`.
Rect menuFrame(Vec2 at, Vec2 size, Rect card);

// Adds the open menu's panel and items to the scene, its items first in
// `elements`, so hitTest and hover find them before the card under it.
void placeMenu(const IslandState& state, Scene& scene, const MeasureText& measure);

} // namespace cup
