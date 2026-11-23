#pragma once
// What Windows' performance counters say, made into a reading's values
// (decisions 173, 174). Pure: the Windows reader collects the counters and
// hands their instances here as they came, so the rules (what a GPU's load
// is, which GPU is the reading's) are tested on any OS. Nothing is cleaned
// here: garbage goes on to stats_format.h (decision 171).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

// One instance of a wildcard counter: a GPU engine and its percent, a thermal zone and its kelvins.
struct CounterValue {
    std::string instance; // UTF-8, as PDH names it
    double value = 0;
    bool operator==(const CounterValue&) const = default;
};

// --- CPU ---
// Busy share of a time span, from GetSystemTimes' deltas (kernel includes
// idle): the load's fallback where the counters are broken. None for an empty span.
std::optional<float> busyPercent(std::uint64_t idle, std::uint64_t kernel, std::uint64_t user);

// --- GPU: \GPU Engine(*)\Utilization Percentage ---
// "pid_1234_luid_0x00000000_0x0000C5B5_phys_0_eng_3_engtype_VideoDecode":
// a process's use of one of an adapter's engines.
struct GpuEngineInstance {
    std::uint64_t adapter = 0; // the luid, high part first
    std::uint32_t engine = 0;  // eng_3: which of the adapter's engines
    bool operator==(const GpuEngineInstance&) const = default;
};
std::optional<GpuEngineInstance> parseGpuEngine(std::string_view instance);

// An adapter's load as Task Manager counts it: an engine's load is its
// processes' together, the adapter's is its busiest engine's (engines work
// side by side: adding 3D to video decode would pass 100).
struct GpuLoad {
    std::uint64_t adapter = 0;
    double load = 0; // percent
    bool operator==(const GpuLoad&) const = default;
};
std::vector<GpuLoad> gpuLoads(const std::vector<CounterValue>& engineInstances); // by luid

// A graphics adapter as DXGI lists it.
struct GpuAdapter {
    std::uint64_t luid = 0;
    std::uint64_t dedicatedMemory = 0; // bytes
    bool software = false;             // Microsoft Basic Render Driver: not a GPU
    bool operator==(const GpuAdapter&) const = default;
};
// The reading's GPU: the busiest hardware adapter, the one with the most
// memory among equals (idle or not measured, the discrete card rather than
// the integrated one). No hardware adapter: none, and the GPU tile goes.
const GpuAdapter* chooseGpu(const std::vector<GpuAdapter>& adapters, const std::vector<GpuLoad>& loads);
// Its load: 0 for one the counters don't list (nothing is using it).
float gpuLoadOf(const GpuAdapter& adapter, const std::vector<GpuLoad>& loads);

// --- Temperature: \Thermal Zone Information(*)\High Precision Temperature ---
// The hottest zone in °C, from tenths of a kelvin (or whole kelvins:
// `perKelvin` 1). None without zones. Zones that read 0 K don't count.
std::optional<float> hottestZone(const std::vector<CounterValue>& zones, double perKelvin);

} // namespace cup
