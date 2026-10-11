#pragma once
// Expanded's tabs (decision 007): with several activities, one tab each
// along the top of the card (icon or art and a short title), built from the
// compact presentation providers already give, so they need no extra work.

#include "core/layout.h"

#include <optional>
#include <vector>

namespace cup {

// How much taller the Expanded card gets for its tab strip (strip + gap below), 0 without tabs.
float tabStripSpace(const IslandState& state);

// The strip, with each tab's content appended to `elements` (pill coordinates).
// Nothing unless Expanded with at least two activities.
// `measure` sizes titles so a tab's icon and title can be centred as a group.
std::optional<TabStrip> tabStripFor(const IslandState& state, Vec2 cardSize, float nestedRadius, const MeasureText& measure,
                                    std::vector<Placed>& elements);

// The tab under `point` (pill coordinates), if any.
const Tab* tabAt(const TabStrip& strip, Vec2 point);

} // namespace cup
