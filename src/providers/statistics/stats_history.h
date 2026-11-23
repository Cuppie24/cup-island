#pragma once
// The last while of each graphed measurement (decision 172), pure: the
// provider records every reading here and the pages draw from it. Values
// are kept as read (percent, °C, bytes per second), cleaned; a reading
// that's missing is kept as a gap, not skipped and not made up, so a graph's
// time axis stays true and its line breaks where nothing was read.

#include "providers/statistics/stats_reading.h"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace cup {

enum class Series { CpuLoad, CpuTemperature, GpuLoad, GpuTemperature, Memory, Download, Upload, DiskRead, DiskWrite };
inline constexpr std::size_t kSeriesCount = 9;

struct StatsHistory {
    using Samples = std::vector<std::optional<float>>; // oldest first; none: a gap
    std::array<Samples, kSeriesCount> series;
    bool operator==(const StatsHistory&) const = default;
};

// Adds `reading` as the newest sample of every series; each keeps its last
// statsHistoryLength.
void record(StatsHistory& history, const StatsReading& reading);
const StatsHistory::Samples& samplesOf(const StatsHistory& history, Series series);
// Whether it has any reading at all: a graph of only gaps isn't worth a tile.
bool anyReading(const StatsHistory::Samples& samples);

// A series as a Graph's samples (0..1, a gap below 0):
// a percent of 100;
std::vector<float> percentSamples(const StatsHistory::Samples& samples);
// a temperature between statsGraphMinTemperature and statsGraphMaxTemperature;
std::vector<float> temperatureSamples(const StatsHistory::Samples& samples);
// a speed of the series' own peak, so download and upload each fill their
// graph; the peak counts as at least statsSpeedFloor, so an idle line lies flat.
std::vector<float> speedSamples(const StatsHistory::Samples& samples);
// Where statsHighTemperature is on a temperature graph, 0..1: its limit line.
float temperatureLimit();

} // namespace cup
