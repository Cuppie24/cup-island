#pragma once
// How big content items are (PS §5.1): heights, fixed widths, text sizes.
// Shared by slot layout and lists (decision 029).

#include "core/layout.h"

#include <optional>
#include <string>

namespace cup {

float lineHeight(TextStyle style); // a line of text in this style
bool isRing(const Item& item);

// Natural height of an item. Images fill the slot's square; a ring is
// `ringSide` (its row's height, or the slot's square when alone).
float heightOf(const Item& item, Rect slot, float ringSide);

// Fixed width, or nothing for items that stretch (Text, TextInput, a bar).
std::optional<float> fixedWidthOf(const Item& item, float height, const MeasureText& measure);

} // namespace cup
