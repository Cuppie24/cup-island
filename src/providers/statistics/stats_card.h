#pragma once
// The statistics card and pill, pure (decision 174): a grid of tiles, one
// reading each, with nothing to click, and in the pill the readings picked
// in Settings. What's shown is a StatsView. One small function per tile:
// easy to rearrange.

#include "core/island.h"
#include "providers/statistics/stats_format.h"
#include "providers/statistics/stats_pill.h"
#include "providers/statistics/stats_reading.h"

#include <vector>

namespace cup {

inline constexpr const char* kStatisticsPanelId = "statistics.system"; // its page, and its activity while the pill shows something

struct StatsView {
    StatsReading reading;                 // the last one; what it wasn't asked for is empty
    std::vector<BluetoothDevice> devices; // the last listing: kept between the readings that list them
    StatsPicks picks;                     // what the pill shows
    bool operator==(const StatsView&) const = default;
};

// The top row: the load big over its level, a detail at the right (a
// temperature; the CPU's cores where there's none; memory in gigabytes).
StatTile cpuTile(const StatsReading& reading);
StatTile gpuTile(const StatsReading& reading);
// `roomy`: two tiles on top, so used of total fits ("8.7 / 16 GB"); else what's used ("16.5 GB").
StatTile memoryTile(const StatsReading& reading, bool roomy);
// The small ones: the system drive's level; both of the network's speeds.
StatTile diskTile(const StatsReading& reading);
StatTile networkTile(const StatsReading& reading);
// Devices' batteries, the emptiest first: as many rows as fit, or one fewer
// and "+N more"; "No devices connected" with none (the tile stays, so the
// card doesn't rearrange when headphones connect).
BatteryTile bluetoothTile(const std::vector<BluetoothDevice>& devices);
// Whether the top row has a GPU tile: only with a GPU reading.
bool hasGpu(const StatsReading& reading);

// The card: CPU, GPU and memory across the top; under them the disk over
// the network at the left, Bluetooth at the right. One height with every
// card (expandedSize.y). With something picked that has a reading, also the
// pill: a gauge and pillText.
Content statisticsContent(const StatsView& view);
// Whether that content has a pill: statistics is then a running activity.
bool hasPill(const Content& content);
// Its page in the page row: the card under a gauge (decision 165).
Panel statisticsPanel(const StatsView& view);

// A device kind's glyph; the plain Bluetooth one for any other.
Glyph glyphOf(DeviceKind kind);

} // namespace cup
