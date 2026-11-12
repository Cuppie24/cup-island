#pragma once
// How big content items are (PS §5.1): heights, fixed widths, text sizes.
// Shared by slot layout and lists (decision 029).

#include "core/layout.h"

#include <optional>
#include <string>

namespace cup {

float lineHeight(TextStyle style); // a line of text in this style
// The style of a Text::detail line under text in `style` (decision 079).
TextStyle detailStyleOf(TextStyle style);
bool isRing(const Item& item);
// A primary button's circle (decision 064) and the glyph in it; a compact
// one is the main action in a row of controls, media's ▶ (decision 094).
float primarySize(const Button& button);
float primaryGlyphOf(const Button& button);
// A Text's width on one line: its value's, or its detail's under it if wider (decision 090).
float textWidth(const Text& text, const MeasureText& measure);
// A Dial's diameter: its widest time and its label, as a block, fit inside the circle, dialInset in (decision 065).
float dialDiameter(const Dial& dial, const MeasureText& measure);

// Natural height of an item. Images fill the slot's square; a ring is
// `ringSide` (its row's height, or the slot's square when alone).
float heightOf(const Item& item, Rect slot, float ringSide, const MeasureText& measure = {});

// Fixed width, or nothing for items that stretch (Text, TextInput, a bar).
std::optional<float> fixedWidthOf(const Item& item, float height, const MeasureText& measure);

} // namespace cup
