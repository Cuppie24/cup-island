#pragma once
// Home (decision 009): a grid of tiles, the clock first (once synced), then
// every panel in the order it was published. A running panel's tile shows
// its activity's tile slot, so the tile stays in place and goes live.

#include "core/layout.h"

#include <vector>

namespace cup {

// Lays out Home's tiles in the card's content area (`cardSize`: without the
// tab strip below it): appends each tile's content to `elements` and returns
// the tile frames. Tiles that don't fit are left out.
std::vector<Tile> homeTilesFor(const IslandState& state, Vec2 cardSize, float tileRadius, Ms now,
                               std::vector<Placed>& elements);

} // namespace cup
