#pragma once
// Expanded's page row (decision 165): along the top of the card (decision
// 047), an icon per page centred in it, always the same ones: every panel
// with a card, running or not, then activities that aren't panels. The
// shown page's icon is lit, the others grey. Home's button sits at the left
// edge (decision 009) and panels' buttons (Panel::rowButton, decision 113)
// beside it or in the right corner: Search by Home, the theme switch by Settings.

#include "core/layout.h"

#include <optional>
#include <vector>

namespace cup {

// How much taller the Expanded card gets for its page row (row + gap under it, decision 047), 0 without one.
float tabStripSpace(const IslandState& state);

// The row, with each button's glyph appended to `elements` (pill coordinates).
// Nothing unless Expanded with somewhere else to go: Home's widgets, or two pages.
std::optional<TabStrip> tabStripFor(const IslandState& state, Vec2 cardSize, float nestedRadius, const MeasureText& measure,
                                    std::vector<Placed>& elements);

// The button under `point` (pill coordinates), if any.
const Tab* tabAt(const TabStrip& strip, Vec2 point);

} // namespace cup
