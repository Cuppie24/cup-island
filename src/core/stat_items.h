#pragma once
// Items that show a measurement (decision 172): a history graph, a strip of
// upright levels, a battery, and two tiles of an overview. Each is one item
// the host lays out and paints as one piece, like a Dial: the content model
// stays flat. Where their parts go is in core/stat_parts.h. Pure data;
// content.h includes this for its Item.

#include <optional>
#include <string>
#include <vector>

namespace cup {

enum class Glyph; // content.h

// How tall a Graph is (tokens): a tile of an overview's top row, one of its
// bottom row, or a strip under a big number.
enum class GraphSize { Tall, Low, Strip };

// A measurement's last while, as a line over a filled area, oldest at the
// left. With `tile`, a rounded tileColor background with its texts over the
// plot's top: a whole tile of an overview in one item.
struct Graph {
    // 0..1 of the plot's height, oldest first. Below 0: no reading then; the
    // line and its area break there instead of joining what's either side.
    std::vector<float> samples;
    // How many samples span the plot's width. Fewer than that: they end at
    // the right edge, so a history just begun grows in from the right. 0, or
    // less than there are: the samples span it.
    std::size_t capacity = 0;
    GraphSize size = GraphSize::Tall;
    bool tile = false;
    // Over the tile's top (only with `tile`): a caption at the left, its
    // digits bright (decision 068), a detail beside it, bright; a value at
    // the right, its digits bright and the rest quiet, in Body, or with
    // `valueLarge` in CardTitle: what an overview's tile is about.
    std::string caption;
    std::string detail;
    std::string value;
    bool valueLarge = false;
    bool captionCentred = false; // the caption alone, centred: both of the network's speeds on one line
    // A dashed line across the plot at this height (0..1), in dangerColor:
    // where a temperature turns hot.
    std::optional<float> limit;
    bool alert = false; // its line and area in dangerColor: the reading is high now
    // Set: the whole item is a button, lit under the pointer (hoverColor over
    // it, at its corner radius) and sending this to its provider.
    std::string actionId;
    bool operator==(const Graph&) const = default;
};

// A row of upright bars, each filling from the bottom: a level per core.
// Stretches across its row, levelsHeight tall; the bars share its width.
struct LevelBar {
    float level = 0;    // 0..1
    bool alert = false; // this bar in dangerColor
    // What it is and reads ("Core 3 · 95%"): for the host to show over the
    // bar under the pointer. Not drawn otherwise.
    std::string label;
    bool operator==(const LevelBar&) const = default;
};
struct Levels {
    std::vector<LevelBar> bars;
    bool operator==(const Levels&) const = default;
};

// A battery's outline with its charge filling it from the left. With a
// `label` before it and a `value` after it (both Caption, the value bright)
// it's one reading in one item: an earbud's "Left ▭ 82%".
struct Battery {
    float level = 0;    // 0..1
    bool alert = false; // its fill in dangerColor: low
    std::string label;
    std::string value;
    bool operator==(const Battery&) const = default;
};

// An overview's tile of one level: a glyph and a caption over a bar, the
// value at the top right, a line under the bar ("148 GB free", its digits
// bright). As tall as a Low graph, so it sits in a row with one.
struct LevelTile {
    Glyph glyph{};
    std::string caption;
    std::string value;
    float level = 0;    // 0..1
    bool alert = false; // the bar in dangerColor
    std::string detail;
    std::string actionId; // as a Graph's: the whole tile is a button
    bool operator==(const LevelTile&) const = default;
};

// An overview's tile of a few devices side by side, each a glyph over a
// battery over its percent. As tall as a Low graph.
struct BatteryCell {
    Glyph glyph{};
    float level = 0;
    bool alert = false;
    std::string value; // "82%"
    bool operator==(const BatteryCell&) const = default;
};
struct BatteryTile {
    std::vector<BatteryCell> cells;
    // In place of cells' percent when there are none: "No devices".
    std::string empty;
    std::string actionId;
    bool operator==(const BatteryTile&) const = default;
};

} // namespace cup
