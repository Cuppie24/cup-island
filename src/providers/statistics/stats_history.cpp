#include "providers/statistics/stats_history.h"

#include "core/tokens.h"
#include "providers/statistics/stats_format.h"

#include <algorithm>
#include <functional>

namespace cup {
namespace {

constexpr float kGap = -1; // Graph::samples: below 0 is no reading

// What each series reads, in Series' order.
std::array<std::optional<float>, kSeriesCount> valuesOf(const StatsReading& r)
{
    return { cleanPercent(r.cpuLoad), cleanTemperature(r.cpuTemperature), cleanPercent(r.gpuLoad), cleanTemperature(r.gpuTemperature),
             usedPercent(r.memoryUsed, r.memoryTotal), cleanSpeed(r.networkDown), cleanSpeed(r.networkUp), cleanSpeed(r.diskRead),
             cleanSpeed(r.diskWrite) };
}

// Each sample through `scale`, clamped to 0..1; a gap stays one.
std::vector<float> scaled(const StatsHistory::Samples& samples, const std::function<float(float)>& scale)
{
    std::vector<float> out;
    out.reserve(samples.size());
    for (const std::optional<float>& s : samples) out.push_back(s ? std::clamp(scale(*s), 0.0f, 1.0f) : kGap);
    return out;
}

float temperatureShare(float celsius)
{
    const Tokens& t = tokens();
    float low = static_cast<float>(t.statsGraphMinTemperature), high = static_cast<float>(t.statsGraphMaxTemperature);
    return high > low ? (celsius - low) / (high - low) : 0.0f; // a mistyped range: a flat line, not a division by zero
}

} // namespace

void record(StatsHistory& history, const StatsReading& reading)
{
    std::size_t most = std::max<std::size_t>(tokens().statsHistoryLength, 1);
    auto values = valuesOf(reading);
    for (std::size_t i = 0; i < kSeriesCount; ++i) {
        StatsHistory::Samples& samples = history.series[i];
        samples.push_back(values[i]);
        if (samples.size() > most) samples.erase(samples.begin(), samples.end() - static_cast<std::ptrdiff_t>(most));
    }
}

const StatsHistory::Samples& samplesOf(const StatsHistory& history, Series series)
{
    return history.series[static_cast<std::size_t>(series)];
}

std::optional<float> peakOf(const StatsHistory::Samples& samples)
{
    std::optional<float> peak;
    for (const std::optional<float>& s : samples)
        if (s && (!peak || *s > *peak)) peak = s;
    return peak;
}

bool anyReading(const StatsHistory::Samples& samples)
{
    return std::any_of(samples.begin(), samples.end(), [](const std::optional<float>& s) { return s.has_value(); });
}

std::vector<float> percentSamples(const StatsHistory::Samples& samples)
{
    return scaled(samples, [](float percent) { return percent / 100; });
}

std::vector<float> temperatureSamples(const StatsHistory::Samples& samples)
{
    return scaled(samples, temperatureShare);
}

std::vector<float> speedSamples(const StatsHistory::Samples& samples)
{
    float peak = static_cast<float>(tokens().statsSpeedFloor) * 1024;
    for (const std::optional<float>& s : samples)
        if (s) peak = std::max(peak, *s);
    return scaled(samples, [peak](float speed) { return peak > 0 ? speed / peak : 0.0f; });
}

float temperatureLimit()
{
    return std::clamp(temperatureShare(static_cast<float>(tokens().statsHighTemperature)), 0.0f, 1.0f);
}

} // namespace cup
