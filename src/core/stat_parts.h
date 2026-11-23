#pragma once
// Where the parts of the measurement tiles go (core/stat_items.h, decision
// 174): a tile's lines and bar, a device row's glyph, name, battery and
// percent. The layout gives each tile one frame; the renderer asks here for
// what's inside it, so no geometry is decided twice. Pure.

#include "core/layout.h"

#include <vector>

namespace cup {

// A StatTile in `frame`, tileInset in from its edges.
struct StatTileParts {
    Rect glyph;   // a small icon at the top left, a caption's line square
    Rect caption; // Caption, beside it; zero wide when it doesn't fit beside `aside`
    Rect aside;   // Caption, right-aligned, its digits bright: a large tile's detail, a small one's value
    Rect value;   // a large tile's reading (TextStyle::Stat); zero for a small one
    Rect bar;     // progressHeight tall along the bottom; zero without a level
    float fill = 0; // ...filled this wide, from its left
};
StatTileParts statTileParts(const StatTile& tile, Rect frame, const MeasureText& measure);
float statTileHeight(const StatTile& tile);

// A battery's shape: its body (an outline batteryStroke thick) with the nub
// at its right and the charge inside, the body's left edge at `left`,
// centred on `middle` (a y).
struct BatteryParts {
    Rect body;
    Rect cap;
    Rect fill;
    float bodyRadius = 0; // the outline's corners, and the fill's inside it
    float fillRadius = 0;
};
BatteryParts batteryShape(float level, float left, float middle);
float batteryShapeWidth(); // body, gap and nub

// A BatteryTile in `frame`.
struct BatteryRowParts {
    Rect bubble;  // one device alone: the circle behind its glyph (tileColor); zero otherwise
    Rect glyph;
    Rect name;    // Caption; alone: Body, in the title's colour
    BatteryParts battery;
    Rect value;   // Caption, right-aligned, its digits bright; alone: CardTitle, from the battery's right
};
struct BatteryTileParts {
    std::vector<BatteryRowParts> rows;
    Rect note;        // Caption, centred; zero without one
    bool alone = false; // one device with the tile to itself: the large treatment
};
BatteryTileParts batteryTileParts(const BatteryTile& tile, Rect frame, const MeasureText& measure);
float batteryTileHeight(); // two small StatTiles and the gap between them
// How many lines (devices, and a note under them) fit a BatteryTile.
std::size_t batteryTileLines();

} // namespace cup
