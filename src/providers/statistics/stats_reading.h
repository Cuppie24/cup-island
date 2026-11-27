#pragma once
// One look at the machine: what the statistics card and pill show
// (decisions 171, 174), and what a look is asked for. The contract between
// the Windows reader, which fills a reading, and the pure half, which
// cleans, formats and lays it out. Plain data.
//
// Every value is optional: Windows often can't say (a temperature without
// admin rights or a vendor's API, no GPU counters), and a request often
// doesn't ask (StatsRequest). The reader leaves out what it couldn't read
// or wasn't asked for, and passes on what it got as it is: garbage (NaN,
// -1, 4000 %) is dealt with in stats_format.h, in one tested place, never
// in the reader.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// What one reading should hold. With the card closed only what the pill
// shows is wanted, so the reader opens only those counters; nothing wanted
// is never asked.
struct StatsRequest {
    bool cpuLoad = false;        // ...and the number of cores, which comes with it
    bool cpuTemperature = false;
    bool gpuLoad = false;
    bool gpuTemperature = false;
    bool memory = false;
    bool disk = false;
    bool network = false;
    // List the Bluetooth devices this time. The slowest look, and the one
    // that must stay rare (decision 042): asked every statsBluetoothRefresh
    // with the card shown, once a minute for the pill. Not asked: the
    // reading's `bluetooth` stays empty and the last listing stands.
    bool bluetooth = false;
    bool any() const { return cpuLoad || cpuTemperature || gpuLoad || gpuTemperature || memory || disk || network || bluetooth; }
    bool operator==(const StatsRequest&) const = default;
};

enum class DeviceKind { Other, Headphones, Earbuds, Mouse, Keyboard, Controller, Pen };

// A connected Bluetooth device. Shown only with a battery level.
struct BluetoothDevice {
    std::string name;             // as Windows names it, UTF-8
    std::optional<float> battery; // percent, 0..100
    DeviceKind kind = DeviceKind::Other;
    bool operator==(const BluetoothDevice&) const = default;
};

struct StatsReading {
    std::optional<float> cpuLoad;         // percent of all cores, 0..100
    std::optional<float> cpuTemperature;  // °C
    std::optional<std::uint32_t> cpuCores; // logical processors: shown where there's no temperature
    // All of it absent on a PC with no GPU counters: its tile goes.
    std::optional<float> gpuLoad;         // percent, 0..100: the busiest adapter's
    std::optional<float> gpuTemperature;  // °C
    // Physical memory, bytes: both or neither.
    std::optional<std::uint64_t> memoryUsed;
    std::optional<std::uint64_t> memoryTotal;
    // The system drive, bytes: both or neither.
    std::optional<std::uint64_t> diskUsed;
    std::optional<std::uint64_t> diskTotal;
    // All adapters together, bytes per second, averaged since the last reading.
    std::optional<float> networkDown;
    std::optional<float> networkUp;
    // Connected devices, in any order (shownDevices sorts). None: not listed
    // this time (StatsRequest::bluetooth); an empty list: listed, none connected.
    std::optional<std::vector<BluetoothDevice>> bluetooth;
    bool operator==(const StatsReading&) const = default;
};

} // namespace cup
