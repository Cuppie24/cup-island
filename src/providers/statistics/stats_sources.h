#pragma once
// Where the statistics reader's numbers come from on Windows (decision
// 173): each function is one look at one source, called on the reader's
// thread. Plain Win32, nothing newer than Windows 10 1809 (decision 015),
// nothing that needs admin rights. None throws; one that fails returns
// nothing, and its values stay out of the reading. No Windows types here.
// stats_sources.cpp: the CPU's name, memory, drives, graphics adapters;
// stats_sources_pdh.cpp: performance counters; stats_sources_network.cpp:
// adapters; stats_sources_bluetooth.cpp: the device tree's Bluetooth nodes.

#include "providers/statistics/stats_bluetooth.h"
#include "providers/statistics/stats_counters.h"
#include "providers/statistics/stats_network.h"
#include "providers/statistics/stats_reading.h"

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

    Counter add(const wchar_t* path);                          // "\Memory\Cache Bytes", "\GPU Engine(*)\Utilization Percentage"
    bool collect();                                            // one sample of all of them
    std::optional<double> value(Counter counter) const;        // a single counter's
    // A wildcard's instances with a value; `read` (optional) says whether
    // the counter answered at all (no instances is then an answer: an idle GPU).
    std::vector<CounterValue> values(Counter counter, bool* read = nullptr) const;

private:
    void* query_ = nullptr;
};

// "AMD Ryzen 7 7840U w/ Radeon 780M Graphics", from the registry; "" if it isn't there.
std::string cpuName();
std::size_t coreCount(); // logical cores, all processor groups; at least 1

// The CPU's times since boot, in 100 ns (GetSystemTimes): kernel includes idle.
struct CpuTimes {
    std::uint64_t idle = 0, kernel = 0, user = 0;
};
std::optional<CpuTimes> cpuTimes();

// Physical memory: used and total (both or neither), and what GetPerformanceInfo calls the system cache.
struct Memory {
    std::uint64_t used = 0, total = 0;
    std::optional<std::uint64_t> cache; // standby and the system's own: only where the standby counters aren't read
};
std::optional<Memory> memory();

// Every fixed drive with its label and space, the system's first.
std::vector<DriveReading> drives();

// Graphics adapters (DXGI): names, dedicated memory, which is software.
std::vector<GpuAdapter> gpuAdapters();

// Hardware network adapters with their byte counters (GetIfTable2).
std::vector<AdapterSample> networkAdapters();

// The device tree's Bluetooth nodes, listed once: properties Windows
// already keeps, read from the PnP manager. No watcher, no subscription, no
// Bluetooth API and nothing sent over the radio (decision 042).
std::vector<BluetoothNode> bluetoothNodes();

// The local date as yyyymmdd, when the PC booted (minutes, UTC), and whether that was today.
struct Today {
    std::int64_t day = 0;
    std::int64_t bootMinute = 0;
    bool bootedToday = false;
};
Today today();

std::string utf8(const wchar_t* text); // "" for null

} // namespace cup::stats
