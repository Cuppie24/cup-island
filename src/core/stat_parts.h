#pragma once
// Where the parts of the measurement items go (core/stat_items.h, decision
// 172): a graph's plot and line, a strip's bars, a battery's body and fill,
// a tile's lines. The layout gives each item one frame; the renderer asks
// here for what's inside it, so no geometry is decided twice. Pure.

#include "core/layout.h"

#include <string>
#include <vector>

namespace cup {

float graphHeight(GraphSize size);

// A Graph in `frame`: its plot, and with a tile, the line of its texts over it.
struct GraphParts {
    Rect plot;  // the line and its area; the limit line crosses it
    Rect texts; // caption and detail from its left, the value to its right; zero without a tile
};
GraphParts graphParts(const Graph& graph, Rect frame);
// The line through the samples, in `plot`: one run of points per unbroken
// stretch of readings (a sample below 0 breaks it), left to right. The area
// under a run goes down to the plot's bottom.
std::vector<std::vector<Vec2>> graphLines(const Graph& graph, Rect plot);
// The limit line's height in `plot`, if the graph has one.
std::optional<float> graphLimitY(const Graph& graph, Rect plot);

// A strip's bars across `frame`, left to right, and the filled part of each
// (from its bottom). Narrower than its gaps would allow: no gaps.
struct LevelFrame {
    Rect bar;  // its track, levelsRadius corners
    Rect fill; // clipped to the bar's shape
};
std::vector<LevelFrame> levelFrames(const Levels& levels, Rect frame);
// The bar under `point`, if any: whose label to show (LevelBar::label).
std::optional<std::size_t> levelAt(const Levels& levels, Rect frame, Vec2 point);

// A battery in `frame`: its label, then its body (an outline batteryStroke
// thick) with the nub at its right, the fill inside, then its value.
struct BatteryParts {
    Rect label; // Caption; zero width without one
    Rect body;
    Rect cap;
    Rect fill;
    Rect value; // Caption, bright; zero width without one
};
float batteryWidth(const Battery& battery, const MeasureText& measure);
BatteryParts batteryParts(const Battery& battery, Rect frame, const MeasureText& measure);
// The body, nub and fill alone, the body's left edge at `left`, centred on
// `middle` (a y): a tile's cells draw the same battery.
BatteryParts batteryShape(float level, float left, float middle);

// A LevelTile in `frame`: three lines centred in it, graphInset in.
struct LevelTileParts {
    Rect glyph;   // a small icon, the caption's height
    Rect caption; // Caption
    Rect value;   // Caption, right-aligned, its digits bright
    Rect bar;     // progressHeight tall
    float fill = 0; // ...filled this wide, from its left
    Rect detail;  // Caption, its digits bright
};
LevelTileParts levelTileParts(const LevelTile& tile, Rect frame, const MeasureText& measure);

// A BatteryTile in `frame`: its cells in equal columns, each a glyph
// (iconSize) over a battery over its value, centred.
struct BatteryCellParts {
    Rect glyph;
    BatteryParts battery; // no label or value: the shape
    Rect value;           // Caption, centred
};
std::vector<BatteryCellParts> batteryTileParts(const BatteryTile& tile, Rect frame);

// What a click on this item sends, "" if it isn't one of the items that are
// a button as a whole (a Graph, a LevelTile or a BatteryTile with an action).
std::string tileAction(const Item& item);

// How full a Progress is at `now`, 0..1: its level, else its timeline's.
float fractionOf(const Progress& progress, Ms now);

} // namespace cup
