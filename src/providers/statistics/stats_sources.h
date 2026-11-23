#pragma once
// Where the statistics reader's numbers come from on Windows (decision
// 173): each function is one look at one source, called on the reader's
// thread. Plain Win32, nothing newer than Windows 10 1809 (decision 015),
// nothing that needs admin rights. None throws; one that fails returns
// nothing, and its values stay out of the reading. No Windows types here.
// stats_sources.cpp: cores, memory, the system drive; stats_sources_gpu.cpp:
// graphics adapters and their temperature;
// stats_sources_pdh.cpp: performance counters; stats_sources_network.cpp:
// adapters; stats_sources_bluetooth.cpp: the device tree's Bluetooth nodes.

#include "providers/statistics/stats_bluetooth.h"
#include "providers/statistics/stats_counters.h"
#include "providers/statistics/stats_network.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup::stats {

// A set of performance counters read together (PDH). Rates ("% Processor
// Time", "Bytes/sec") are Windows' average between two collect() calls, so
// they have a value from the second on. Counters are named in English,
// whatever Windows' language. Holds a handle: not copied.
class CounterSet {
public:
    using Counter = void*; // nullptr: this PC has no such counter

    CounterSet();
    ~CounterSet();
    CounterSet(const CounterSet&) = delete;
    CounterSet& operator=(const CounterSet&) = delete;

    Counter add(const wchar_t* path);                          // "\GPU Engine(*)\Utilization Percentage"
    bool collect();                                            // one sample of all of them
    std::optional<double> value(Counter counter) const;        // a single counter's
    // A wildcard's instances with a value; `read` (optional) says whether
    // the counter answered at all (no instances is then an answer: an idle GPU).
    std::vector<CounterValue> values(Counter counter, bool* read = nullptr) const;

private:
    void* query_ = nullptr;
};

std::uint32_t coreCount(); // logical processors, all processor groups; at least 1

// The CPU's times since boot, in 100 ns (GetSystemTimes): kernel includes idle.
struct CpuTimes {
    std::uint64_t idle = 0, kernel = 0, user = 0;
};
std::optional<CpuTimes> cpuTimes();

// Used and total bytes: of physical memory, and of the system drive.
struct Space {
    std::uint64_t used = 0, total = 0;
};
std::optional<Space> memory();
std::optional<Space> systemDrive();

// Graphics adapters (DXGI): which are hardware, and their dedicated memory,
// to choose the reading's GPU by (stats_counters.h).
std::vector<GpuAdapter> gpuAdapters();
// An adapter's temperature in °C as its driver reports it to Windows (what
// Task Manager shows), by its luid. None where the driver or this Windows
// doesn't say. stats_sources_gpu.cpp.
std::optional<float> gpuTemperature(std::uint64_t luid);

// Hardware network adapters with their byte counters (GetIfTable2).
std::vector<AdapterSample> networkAdapters();

// The device tree's Bluetooth nodes, listed once: properties Windows
// already keeps, read from the PnP manager. No watcher, no subscription, no
// Bluetooth API and nothing sent over the radio (decision 042).
std::vector<BluetoothNode> bluetoothNodes();

std::string utf8(const wchar_t* text); // "" for null

} // namespace cup::stats
