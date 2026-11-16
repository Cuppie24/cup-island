#pragma once
// Expanded's page row (decisions 007, 109): along the top of the card
// (decision 047), a dot per activity in start order (Home has none: its
// button is the way there, decision 115), the shown one a short line, with ‹ and › beside the dots, the
// three centred together. No tabs drawn: the card says what it is. With
// panels, Home's button sits apart at the left edge (decision 009); the pin
// mirrors it at the right (decision 046), panels' buttons beyond it in the
// corner (Panel::rowButton, decision 113).

#include "core/layout.h"

#include <optional>
#include <vector>

namespace cup {

// How much taller the Expanded card gets for its tab strip (strip + gap under it, decision 047), 0 without tabs.
float tabStripSpace(const IslandState& state);

// The strip, with each tab's content appended to `elements` (pill coordinates).
// Nothing unless Expanded with somewhere else to go: two activities, or one and Home.
std::optional<TabStrip> tabStripFor(const IslandState& state, Vec2 cardSize, float nestedRadius, const MeasureText& measure,
                                    std::vector<Placed>& elements);

// The dot or button under `point` (pill coordinates), if any.
const Tab* tabAt(const TabStrip& strip, Vec2 point);

} // namespace cup
