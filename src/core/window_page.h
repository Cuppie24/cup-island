#pragma once
// A window's page laid out (decisions 104, 117): its header across the pane,
// its title, then its sections as groups of rows in one column, or as cards
// side by side. Part of windowLayout; pure.

#include "core/layout.h"
#include "core/slot_layout.h"
#include "core/window_content.h"

#include <optional>
#include <vector>

namespace cup {

// Where the cards of a WindowPage::cards page go: as many windowCardWidth
// columns as fit in `available`, windowCardGap apart. One width whatever the
// window's, so a resize only moves cards (decision 117); narrower than one
// card, the one column is as wide as there is.
struct CardGrid {
    int columns = 1;
    float width = 0; // each card's
};
CardGrid cardGrid(float available);

// Each card's frame, in order, row by row from `top` at `left`: a row as
// tall as its tallest card. `heights`: each card's.
std::vector<Rect> cardFrames(const CardGrid& grid, const std::vector<float>& heights, Vec2 topLeft);

// The page's items in `out` (page coordinates: the pane's top-left
// unscrolled), `paneWidth` wide. Returns how tall it is, padding included.
// `rule`: the line under the header, if it has one.
float placePage(const WindowPage& page, float paneWidth, const Source& fields, Ms now, const MeasureText& measure, std::vector<Placed>& out,
                std::optional<Rect>& rule);

// A window's text a step up from the island's (decision 112): Title, Body
// and Caption become their Window kin. Providers write the plain styles, so
// the same rows read the same on a card and in a window.
Slot windowStyled(Slot slot);

} // namespace cup
