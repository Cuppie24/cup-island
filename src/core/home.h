#pragma once
// Home (decisions 009, 019): one row of widgets across the card's full
// width, one per panel, in the order they were published. A running panel's widget shows its activity's tile,
// so it stays in place and goes live.

#include "core/layout.h"

#include <vector>

namespace cup {

// Lays out Home's tiles in the card's content area (`cardSize`: without the
// tab strip below it): appends each tile's content to `elements` and returns
// the tile frames. Widgets that don't fit the row are left out.
std::vector<Tile> homeTilesFor(const IslandState& state, Vec2 cardSize, float tileRadius, Ms now,
                               std::vector<Placed>& elements);

} // namespace cup
