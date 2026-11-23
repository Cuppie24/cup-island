#pragma once
// Tiles that show a measurement (decision 174): one reading with its level,
// and a few devices' batteries. Each is one item the host lays out and
// paints as one piece, like a Dial: the content model stays flat, no item
// holds items. Where their parts go is in core/stat_parts.h. Pure data;
// content.h includes this for its Item.

#include <optional>
#include <string>
#include <vector>

namespace cup {

enum class Glyph; // content.h

// One reading on a rounded tileColor background: a glyph and a caption at
// its top left, a level bar along its bottom. Stretches across its share of
// a row. Nothing to click.
//  large: `detail` at the top right ("46°", "16.5 GB", its digits bright),
//         `value` big between the two (TextStyle::Stat); statTileHeight tall.
//  else:  `value` at the top right (Caption, its digits bright), no detail;
//         statMiniHeight tall, and without a level only the one line.
// A caption that doesn't fit beside what's at the right is left out whole,
// not cut: the glyph still says what the tile is.
struct StatTile {
    Glyph glyph{};
    std::string caption;
    std::string detail;
    std::string value;
    std::optional<float> level; // 0..1; none: no bar
    bool alert = false;         // the bar in dangerColor: this reading is high
    bool large = false;
    bool operator==(const StatTile&) const = default;
};

// A device and its battery, in a BatteryTile.
struct BatteryRow {
    Glyph glyph{};
    std::string name;
    float level = 0;    // 0..1
    bool alert = false; // its charge in dangerColor: low
    std::string value;  // "82%"
    bool operator==(const BatteryRow&) const = default;
};

// A tile of devices' batteries, as tall as two small StatTiles with their
// gap. A row each: glyph, name, a battery's shape, its percent. One device
// alone gets the tile to itself: its glyph in a circle, its name, its
// battery and percent bigger under it.
struct BatteryTile {
    std::vector<BatteryRow> rows;
    // A quiet line (Caption, centred): alone when there are no rows ("No
    // devices connected"), else in a row's place under them ("+2 more").
    std::string note;
    bool operator==(const BatteryTile&) const = default;
};

} // namespace cup
