#pragma once
// Home (decisions 009, 019): rows of widgets, one per panel, in the order
// they were published; every row reaches across the card's full width. A running panel's widget shows its activity's tile,
// so it stays in place and goes live.

#include "core/layout.h"

#include <vector>

namespace cup {

// Lays out Home's tiles in the card's content area (`cardSize`: without the
// tab strip below it): appends each tile's content to `elements` and returns
// the tile frames. None is left out: the card grows to fit (homeCardHeight).
std::vector<Tile> homeTilesFor(const IslandState& state, Vec2 cardSize, float tileRadius, Ms now,
                               std::vector<Placed>& elements);

// How tall Home's card is (without the tab strip): Expanded's height, or
// taller to fit every row of widgets, up to `maxHeight` (the canvas).
float homeCardHeight(const IslandState& state, float maxHeight);

} // namespace cup
