#pragma once
// What Windows' performance counters say, made into a reading's values
// (decision 173). Pure: the Windows reader collects the counters and hands
// their instances here as they came, so the rules (which instance is a
// core, what a GPU's load is, whose process it was) are tested on any OS.
// Nothing is cleaned here: garbage goes on to stats_format.h (decision 171).

#include "providers/statistics/stats_reading.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

// One instance of a wildcard counter: "0,3" and its percent, "chrome" and its bytes.
struct CounterValue {
    std::string instance; // UTF-8, as PDH names it
    double value = 0;
    bool operator==(const CounterValue&) const = default;
};

// --- CPU: \Processor Information(*)\% Processor Utility ---
struct CpuLoads {
    std::optional<float> total; // the "_Total" instance
    std::vector<float> cores;   // "0,0", "0,1", "1,0"...: by processor group, then number
    bool operator==(const CpuLoads&) const = default;
};
// Instances are "group,number" (Processor Information) or "number"
// (Processor, the old object); "0,_Total" (a group's) is left out.
CpuLoads cpuLoads(const std::vector<CounterValue>& instances);
// Busy share of a time span, from GetSystemTimes' deltas (kernel includes
// idle): the total's fallback where the counters are broken. None for an empty span.
std::optional<float> busyPercent(std::uint64_t idle, std::uint64_t kernel, std::uint64_t user);

// --- GPU: \GPU Engine(*)\Utilization Percentage ---
// "pid_1234_luid_0x00000000_0x0000C5B5_phys_0_eng_3_engtype_VideoDecode".
struct GpuEngineInstance {
    std::uint32_t pid = 0;
    std::uint64_t adapter = 0; // the luid, high part first
    std::uint32_t engine = 0;  // eng_3: which of the adapter's engines
    std::string type;          // "VideoDecode"
    bool operator==(const GpuEngineInstance&) const = default;
};
std::optional<GpuEngineInstance> parseGpuEngine(std::string_view instance);
// The luid of "luid_0x00000000_0x0000C5B5_phys_0" (GPU Adapter Memory), or of an engine's instance.
std::optional<std::uint64_t> parseAdapterLuid(std::string_view instance);
// "Video decode" from "VideoDecode", "Compute 0" from "Compute_0"; "3D" stays.
std::string engineName(std::string_view type);

struct PidValue {
    std::uint32_t pid = 0;
    double value = 0;
    bool operator==(const PidValue&) const = default;
};
// An adapter's load as Task Manager counts it: an engine's load is its
// processes' together, the adapter's is its busiest engine's (engines work
// side by side: adding 3D to video decode would pass 100).
struct GpuLoad {
    std::uint64_t adapter = 0;
    double load = 0;                 // percent
    std::vector<NamedValue> engines; // by type, the busiest engine of each: "3D", "Video decode"
    std::vector<PidValue> processes; // a process's busiest engine
    bool operator==(const GpuLoad&) const = default;
};
std::vector<GpuLoad> gpuLoads(const std::vector<CounterValue>& engineInstances); // by luid

// A graphics adapter as DXGI describes it.
struct GpuAdapter {
    std::uint64_t luid = 0;
    std::string name;
    std::uint64_t dedicatedMemory = 0; // bytes
    bool software = false;             // Microsoft Basic Render Driver: not a GPU
    bool operator==(const GpuAdapter&) const = default;
};
// The reading's GPU: the busiest hardware adapter, the one with the most
// memory among equals (idle, the discrete card rather than the integrated
// one). `loads` empty with `countersRead`: an idle GPU, 0 %. Not read: only
// the name and memory. No hardware adapter: nothing, and the GPU tile goes.
// `memoryInstances`: \GPU Adapter Memory(*)\Dedicated Usage.
void fillGpu(StatsReading& reading, const std::vector<GpuAdapter>& adapters, bool countersRead, const std::vector<GpuLoad>& loads,
             const std::vector<CounterValue>& memoryInstances, const std::vector<CounterValue>& processIds);

// --- Processes: \Process(*)\... ---
// "chrome" from "chrome#3" (how PDH tells same-named processes apart).
std::string processName(std::string_view instance);
// An app's processes together, by name. "_Total" and "Idle" aren't apps.
// `divisor`: % Processor Time is of one core, a reading's of all (the core count); 1 for bytes.
std::vector<NamedValue> byProcessName(const std::vector<CounterValue>& instances, double divisor);
// GPU processes named: `processIds` is \Process(*)\ID Process (the instance
// is the name, the value the pid). A pid with no name (it just ended) is left out.
std::vector<NamedValue> namedGpuProcesses(const std::vector<PidValue>& loads, const std::vector<CounterValue>& processIds);

// --- Temperature: \Thermal Zone Information(*)\High Precision Temperature ---
// The hottest zone in °C, from tenths of a kelvin (or whole kelvins:
// `perKelvin` 1). None without zones. Zones that read 0 K don't count.
std::optional<float> hottestZone(const std::vector<CounterValue>& zones, double perKelvin);

} // namespace cup
