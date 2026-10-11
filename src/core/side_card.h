#pragma once
// Expanded's side card (decision 007): every activity except the one shown,
// as one row each (icon or art, a title, a short status), built from the
// compact presentation providers already give, so they need no extra work.

#include "core/layout.h"

#include <optional>

namespace cup {

// Nothing unless Expanded with at least two activities. `card` is the
// Expanded card's frame (render-layer coordinates); the side card sits a
// bubbleGap to its right.
std::optional<SideCard> sideCardFor(const IslandState& state, Rect card);

// The row under `point` (render-layer coordinates), if any.
const SideRow* sideRowAt(const SideCard& side, Vec2 point);

} // namespace cup
