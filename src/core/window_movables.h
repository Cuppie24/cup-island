#pragma once
// What a window's page lets the user drag (decision 129), as laid out: rows
// with a Row::moveAction in a card, and cards with a WindowSection::moveAction
// (by their handle), and the cards a dragged row may be dropped in. Pure data;
// window_drag.h works with it.

#include "core/geometry.h"

#include <cstddef>
#include <string>

namespace cup {

struct Movable {
    bool card = false;     // a whole card, by its handle; else a row in a card
    std::string key;       // its items' key prefix: "<slot>" for a card, "<slot>/<row id>" for a row
    std::string section;   // its card's WindowSection::id
    std::size_t index = 0; // its place among its kind: movable cards on the page, movable rows in its card
    Rect frame;            // page coordinates
    std::string action;    // the move's action (Row::moveAction, WindowSection::moveAction)
    bool operator==(const Movable&) const = default;
};

// A card a row may be dropped in: its rows' own, or one that takes rows (WindowSection::takesRows).
struct DropZone {
    std::string section;
    std::string slot;
    Rect frame; // page coordinates
    bool operator==(const DropZone&) const = default;
};

} // namespace cup
