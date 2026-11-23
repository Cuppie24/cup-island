#pragma once
// One look at the machine: what the statistics card shows (decision 171).
// The contract between the Windows reader, which fills it, and the pure
// half, which cleans, formats and lays it out. Plain data.
//
// Every value is optional: Windows often can't say (a temperature without
// admin rights or a vendor's API, a desktop's battery, no GPU counters).
// The reader leaves out what it couldn't read and passes on what it got as
// it is: garbage (NaN, -1, 4000 %) is dealt with in stats_format.h, in one
// tested place, never in the reader.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// A connected Bluetooth device. Only ones with a battery level are shown.
struct BluetoothDevice {
    std::string name;             // as Windows names it, UTF-8
    std::optional<float> battery; // percent, 0..100
    bool operator==(const BluetoothDevice&) const = default;
};

struct StatsReading {
    std::optional<float> cpuLoad;        // percent of all cores, 0..100
    std::optional<float> cpuTemperature; // °C
    std::optional<float> gpuLoad;        // percent, 0..100: the busiest adapter's
    std::optional<float> gpuTemperature; // °C
    // Physical memory, bytes. The card works out the percent: both or neither.
    std::optional<std::uint64_t> memoryUsed;
    std::optional<std::uint64_t> memoryTotal;
    // The system drive, bytes; both or neither.
    std::optional<std::uint64_t> diskUsed;
    std::optional<std::uint64_t> diskTotal;
    // All adapters together, bytes per second, averaged since the last reading.
    std::optional<float> networkDown;
    std::optional<float> networkUp;
    std::optional<float> battery; // the laptop's own, percent; none: no battery
    bool charging = false;        // ...and it's on power. Read only with `battery`
    // Connected devices, in any order: the card picks and sorts (shownDevices).
    std::vector<BluetoothDevice> bluetooth;
    bool operator==(const StatsReading&) const = default;
};

} // namespace cup
