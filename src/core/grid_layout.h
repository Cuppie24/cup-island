#pragma once
// Grids of cells (Row::cells, decision 063): wrapped into lines that lay out
// and scroll like a list's rows; each cell's rows stacked in it, its actions
// in its top-right corner, all of it lit and revealed as one (hoverGroup).
// Part of placeSlot(); split out of slot_layout.cpp to keep it readable.

#include "core/slot_layout.h"

#include <map>
#include <string>
#include <vector>

namespace cup {

// How many cells fit across `width`: at least gridCellMinWidth each, gridGap apart; never none.
std::size_t gridColumns(float width);

// The slot with every grid row wrapped into lines of cells, each line a Row
// with `cells` (named "<row id>.<line>"), keeping the grid's list.
struct WrappedGrids {
    Slot rows;
    std::vector<std::size_t> firstCell;           // per row: a line's first cell's index in its grid
    std::map<std::string, std::size_t> columns;   // per list holding a grid: its columns
    bool any = false;                             // nothing to wrap: use the slot as it is
};
WrappedGrids wrapGrids(const Slot& slot, float width);

// Places one line's cells across `frame` (the line's, already scrolled):
// keys "<key>/<cell id>/<row>/<item>", the actions "<key>/<cell id>/actions/<n>".
void placeCells(const Row& line, std::size_t firstCell, Rect frame, const std::optional<InList>& list, const std::string& key,
                const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out, const MeasureText& measure);

} // namespace cup
