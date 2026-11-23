#pragma once
// Expanded's page row (decision 165): along the top of the card (decision
// 047), an icon per page centred in it, always the same ones: every panel
// with a card, running or not, then activities that aren't panels. The
// shown page's icon is lit, the others grey. Panels' buttons
// (Panel::rowButton, decision 113) sit in the corners: Search at the left,
// Settings at the right with the theme switch beside it. No Home (decision 170).

#include "core/layout.h"

#include <optional>
#include <vector>

namespace cup {

// How much taller the Expanded card gets for its page row (row + gap under it, decision 047), 0 without one.
float tabStripSpace(const IslandState& state);

// The row, with each button's glyph appended to `elements` (pill coordinates).
// Nothing unless Expanded with somewhere else to go: two pages, or a panel's button.
std::optional<TabStrip> tabStripFor(const IslandState& state, Vec2 cardSize, float nestedRadius, const MeasureText& measure,
                                    std::vector<Placed>& elements);

// The button under `point` (pill coordinates), if any.
const Tab* tabAt(const TabStrip& strip, Vec2 point);

} // namespace cup
