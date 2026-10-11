#pragma once
// Split's geometry (PS §3.1): the compact pill and its detached bubble, and
// the drag-to-swap rule (decision 007). Shared by reduce() and layout().

#include "core/geometry.h"

namespace cup {

// Render-layer coordinates, from tokens(): the pill centred at the top edge,
// the bubble a gap to its right, vertically centred on it.
Rect splitPillFrame();
Rect bubbleHomeFrame();

// Dragged this far from its home, is the bubble over the pill? (Its centre
// is inside the pill: then letting go swaps them.)
bool bubbleOverPill(Vec2 drag);

// A press on the bubble that has moved this far is a drag, not a click.
bool isDrag(Vec2 drag);

} // namespace cup
