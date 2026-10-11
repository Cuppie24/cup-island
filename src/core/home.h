#pragma once
// Home (decision 009): the Expanded view of every panel, as a grid of tiles
// in the order panels were published. A running panel's tile shows its
// activity's tile slot, so the tile stays in place and goes live.

#include "core/layout.h"

#include <vector>

namespace cup {

// Lays out Home's tiles in the card below `top` (the tab strip, if any):
// appends each tile's content to `elements` and returns the tile frames.
// Tiles that don't fit the card are left out.
std::vector<Tile> homeTilesFor(const IslandState& state, Vec2 cardSize, float top, float tileRadius, Ms now,
                               std::vector<Placed>& elements);

} // namespace cup
