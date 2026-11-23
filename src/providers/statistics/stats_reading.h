#pragma once
// One look at the machine: what the statistics pages show (decisions 171,
// 172). The contract between the Windows reader, which fills it, and the
// pure half, which cleans, formats and lays it out. Plain data.
//
// Every value is optional, every list may be empty: Windows often can't say
// (a temperature without admin rights or a vendor's API, no GPU counters,
// earbuds that report one level). The reader leaves out what it couldn't
// read and passes on what it got as it is: garbage (NaN, -1, 4000 %) is
// dealt with in stats_format.h, in one tested place, never in the reader.
// Lists come in any order and any length: the pages sort and cut them.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// A name with a number: a process and its share, a GPU engine and its load.
struct NamedValue {
    std::string name; // UTF-8, as Windows names it ("Chrome", "Video decode")
    double value = 0; // what the list it's in says: percent, or bytes
    bool operator==(const NamedValue&) const = default;
};

struct DriveReading {
    std::string label; // "Windows (C:)"
    std::optional<std::uint64_t> used;  // bytes; both or neither
    std::optional<std::uint64_t> total;
    bool operator==(const DriveReading&) const = default;
};

enum class DeviceKind { Other, Headphones, Earbuds, Mouse, Keyboard, Controller, Pen };

// A connected Bluetooth device. Shown only with a battery level, its own or its parts'.
struct BluetoothDevice {
    std::string name;             // as Windows names it, UTF-8
    std::optional<float> battery; // percent, 0..100
    DeviceKind kind = DeviceKind::Other;
    // Earbuds that say so: "Left", "Right", "Case", each with its level
    // (`value`, percent). Most report one level only: leave it empty.
    std::vector<NamedValue> parts;
    bool operator==(const BluetoothDevice&) const = default;
};

struct StatsReading {
    // --- CPU ---
    std::optional<float> cpuLoad;        // percent of all cores, 0..100
    std::optional<float> cpuTemperature; // °C
    std::string cpuName;                 // "AMD Ryzen 7 7840U"
    std::vector<float> coreLoads;        // percent, a logical core each, in Windows' order
    std::vector<float> coreTemperatures; // °C, a core each; empty: not reported
    std::vector<NamedValue> cpuProcesses; // percent of all cores

    // --- GPU: all of it absent on a PC with no GPU counters; its tile and page go ---
    std::optional<float> gpuLoad;        // percent, 0..100: the busiest adapter's
    std::optional<float> gpuTemperature; // °C
    std::string gpuName;
    std::optional<std::uint64_t> gpuMemoryUsed;  // dedicated memory, bytes; both or neither
    std::optional<std::uint64_t> gpuMemoryTotal;
    std::vector<NamedValue> gpuEngines;   // "3D", "Video decode": percent
    std::vector<NamedValue> gpuProcesses; // percent

    // --- Memory: physical, bytes ---
    std::optional<std::uint64_t> memoryUsed;  // in use; with the total: both or neither
    std::optional<std::uint64_t> memoryTotal;
    std::optional<std::uint64_t> memoryCached; // standby: held, given up when asked for. Free is what's left
    std::vector<NamedValue> memoryApps;        // bytes, an app's processes together

    // --- Network: all adapters together ---
    std::optional<float> networkDown; // bytes per second, averaged since the last reading
    std::optional<float> networkUp;
    std::optional<std::uint64_t> linkSpeed; // bits per second, the adapter in use
    std::string networkName;                // the Wi-Fi network's, or the adapter's
    bool wired = false;                     // ...which is a cable

    // --- Disks ---
    std::vector<DriveReading> drives;  // every fixed drive, the system's first
    std::optional<float> diskRead;     // bytes per second, all drives together
    std::optional<float> diskWrite;

    // --- Bluetooth: connected devices, in any order (shownDevices picks and sorts) ---
    std::vector<BluetoothDevice> bluetooth;

    bool operator==(const StatsReading&) const = default;
};

} // namespace cup
