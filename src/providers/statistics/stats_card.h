#pragma once
// The statistics card, pure (decision 171): a reading laid out with the
// island's own pieces. Three tiles side by side (CPU, GPU, memory: a name,
// its percent, a bar, a detail under it), a row of the rest (disk, network,
// battery) and a row of Bluetooth devices' batteries.
// One small function per row or tile: the layout is expected to move.

#include "core/island.h"
#include "providers/statistics/stats_format.h"
#include "providers/statistics/stats_reading.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

inline constexpr const char* kStatisticsPanelId = "statistics.system"; // its panel: a page, never an activity

// One of the card's main readings: a column of the table.
struct StatTile {
    std::string label;            // "CPU"
    std::optional<float> percent; // cleaned; none: "—" over an empty bar
    std::string detail;           // under the bar: a temperature, used / total; may be ""
    bool operator==(const StatTile&) const = default;
};
StatTile cpuTile(const StatsReading& reading);
StatTile gpuTile(const StatsReading& reading);
StatTile memoryTile(const StatsReading& reading);

// The tiles as a table (Row::even): their names, percents, bars and
// details, a row each, so columns line up whatever a value's width.
Slot tileRows(const std::vector<StatTile>& tiles);
// A bar filled to `percent`, standing still; none: an empty track.
Progress levelBar(std::optional<float> percent);

// "Disk 71%", "↓ 1.2 MB/s", "↑ 340 KB/s", "Battery 82%" ("Charging 82%" on
// power): only those with a reading, in that order.
std::vector<std::string> otherTexts(const StatsReading& reading);
// ...in equal columns. No row (nullopt) with none of them.
std::optional<Row> otherRow(const StatsReading& reading);
// "AirPods Pro 80%" for each device shownDevices picks, the percent bright,
// then "+2 more"; nullopt with no device to show.
std::optional<Row> bluetoothRow(const StatsReading& reading);

// The whole card. An empty reading (nothing read yet) is the same card with
// "—" for every percent. One height with every card (expandedSize.y).
Content statisticsContent(const StatsReading& reading);
// Its page: the card under a gauge in the page row (decision 165).
Panel statisticsPanel(const StatsReading& reading);

} // namespace cup
