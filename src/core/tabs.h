#pragma once
// Expanded's tabs (decision 007): with several activities, one tab each
// (icon or art and a short title) along the top of the card (decision 047), built from
// the compact presentation providers already give, so they need no extra
// work. With panels, a square Home button sits apart at the left (decision 009);
// the pin mirrors it at the right (decision 046).

#include "core/layout.h"

#include <optional>
#include <vector>

namespace cup {

// How much taller the Expanded card gets for its tab strip (strip + gap under it, decision 047), 0 without tabs.
float tabStripSpace(const IslandState& state);

// The strip, with each tab's content appended to `elements` (pill coordinates).
// Nothing unless Expanded with somewhere else to go: two activities, or one and Home.
// `measure` sizes titles so a tab's icon and title can be centred as a group.
std::optional<TabStrip> tabStripFor(const IslandState& state, Vec2 cardSize, float nestedRadius, const MeasureText& measure,
                                    std::vector<Placed>& elements);

// The tab under `point` (pill coordinates), if any.
const Tab* tabAt(const TabStrip& strip, Vec2 point);

} // namespace cup
