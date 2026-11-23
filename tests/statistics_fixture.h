#pragma once
// A reading with everything on it, as the mockup's laptop, and a view of it
// laid out as its card. For the statistics tests.

#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/statistics/stats_card.h"

#include <string>
#include <vector>

namespace cup::test {

inline constexpr std::uint64_t kMb = 1024ull * 1024, kGb = 1024 * kMb;

inline std::vector<BluetoothDevice> fullDevices()
{
    return { { "MX Master 3S", 60.0f, DeviceKind::Mouse }, { "WH-1000XM5", 82.0f, DeviceKind::Headphones }, { "Keychron K3", 45.0f, DeviceKind::Keyboard } };
}

// A laptop with everything readable.
inline StatsReading fullReading()
{
    StatsReading r;
    r.cpuLoad = 14, r.cpuTemperature = 46, r.cpuCores = 16;
    r.gpuLoad = 13, r.gpuTemperature = 42;
    r.memoryUsed = kGb * 165 / 10, r.memoryTotal = 32 * kGb;
    r.diskUsed = 364 * kGb, r.diskTotal = 512 * kGb;
    r.networkDown = 9.9f * kMb, r.networkUp = 0.3f * kMb;
    r.bluetooth = fullDevices();
    return r;
}

// What the provider would hold after `reading`.
inline StatsView viewOf(const StatsReading& reading, StatsPicks picks = {})
{
    return { .reading = reading, .devices = reading.bluetooth.value_or(std::vector<BluetoothDevice>{}), .picks = std::move(picks) };
}

inline Scene shown(const StatsView& view)
{
    IslandState state;
    state = reduce(state, PanelPublished{ statisticsPanel(view) }, Ms{ 0 });
    state = reduce(state, PillClicked{}, Ms{ 0 }); // the idle pill opens the first page (decision 170)
    return layout(state, Ms{ 0 });
}

inline const Placed* placedAt(const Scene& scene, const std::string& key)
{
    for (const Placed& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

} // namespace cup::test
