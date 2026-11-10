#pragma once
// Grouped rows (decision 090): a small, quiet label over rows that sit on one
// rounded background, each row a name with an optional description under it
// and a control at its end. Any card can use them: Settings' pages are made
// of them. Plain Row builders; the host draws the group (Row::grouped).

#include "core/content.h"

#include <string>
#include <vector>

namespace cup {

// A row's two levels: its name, and a quieter line under it ("Version
// 0.6.14"). Empty: the name alone, one line.
Text itemText(std::string name, std::string description = "");

// One of a group's rows: its text, then its control at the end (a chevron,
// a switch, a glyph). A Button or Switch there takes clicks anywhere on
// the row and lights it; anything else leaves the row plain (a fixed key).
Row groupItem(Text text, std::vector<Item> end = {});

// A group's label (TextStyle::Heading): sectionGap above it unless it's the slot's first row.
Row groupLabel(std::string label, bool first);

// Appends a group to `slot`: its label (none if empty), then its rows on one
// background. Nothing at all if there are no rows. `list`: the slot's
// scrolling list they go in (Row::list), if any.
void addGroup(Slot& slot, const std::string& label, std::vector<Row> rows, const std::string& list = "");

} // namespace cup
