#pragma once
// A window's page laid out (decisions 104, 117): its header across the pane,
// its title, then its sections as groups of rows in one column, or as cards
// side by side. Part of windowLayout; pure.

#include "core/layout.h"
#include "core/slot_layout.h"
#include "core/window_content.h"
#include "core/window_movables.h"

#include <optional>
#include <vector>

namespace cup {

// Where the cards of a WindowPage::cards page go: as many columns as fit in
// `available` at windowCardMinWidth or more, windowCardGap apart, sharing the
// row's width (decision 120: fixed widths left them small and a gap at the
// right); narrower than one card, the one column is as wide as there is.
struct CardGrid {
    int columns = 1;
    float width = 0; // each card's
};
CardGrid cardGrid(float available);

// Each card's frame, in order, row by row from `top` at `left`: a row as
// tall as its tallest card, one short of cards centred (decision 123). `heights`: each card's.
std::vector<Rect> cardFrames(const CardGrid& grid, const std::vector<float>& heights, Vec2 topLeft);

// A page laid out (placePage): how tall it is, padding included; the line
// under its header, if it has one; and what can be dragged on it (decision 129).
struct PlacedPage {
    float height = 0;
    std::optional<Rect> rule;
    std::vector<Movable> movables;
    std::vector<DropZone> dropZones;
};

// The page's items in `out` (page coordinates: the pane's top-left
// unscrolled), `paneWidth` wide. `accent`: the window's, for a header's
// progress (WindowPage::headerProgress).
PlacedPage placePage(const WindowPage& page, float paneWidth, const Source& fields, Ms now, const MeasureText& measure, std::vector<Placed>& out,
                     std::optional<Color> accent = std::nullopt);

// The key of a card's drag handle (WindowSection::moveAction): "<slot>/handle".
inline constexpr const char* kWindowDragHandle = "window.drag"; // its action: never sent, a press on it starts a drag

// A window's text a step up from the island's (decision 112): Title, Body
// and Caption become their Window kin. Providers write the plain styles, so
// the same rows read the same on a card and in a window.
Slot windowStyled(Slot slot);

} // namespace cup
