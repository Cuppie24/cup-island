#include "providers/statistics/stats_counters.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <utility>

namespace cup {
namespace {

// A whole number in `base`, all of `text`; none for anything else ("_Total", "").
std::optional<std::uint64_t> number(std::string_view text, int base = 10)
{
    std::uint64_t value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (error != std::errc{} || end != text.data() + text.size() || text.empty()) return std::nullopt;
    return value;
}

// What follows `key` in `text` up to the next '_'.
std::optional<std::string_view> after(std::string_view text, std::string_view key)
{
    std::size_t at = text.find(key);
    if (at == std::string_view::npos) return std::nullopt;
    std::string_view rest = text.substr(at + key.size());
    return rest.substr(0, rest.find('_'));
}

std::optional<std::uint64_t> hexNumber(std::string_view text)
{
    if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
    return number(text, 16);
}

// The luid of "…luid_0x<high>_0x<low>…".
std::optional<std::uint64_t> parseAdapterLuid(std::string_view instance)
{
    std::size_t at = instance.find("luid_");
    if (at == std::string_view::npos) return std::nullopt;
    std::string_view rest = instance.substr(at + 5);
    std::size_t split = rest.find('_');
    if (split == std::string_view::npos) return std::nullopt;
    std::string_view second = rest.substr(split + 1);
    auto high = hexNumber(rest.substr(0, split));
    auto low = hexNumber(second.substr(0, second.find('_')));
    if (!high || !low) return std::nullopt;
    return (*high << 32) | (*low & 0xFFFFFFFFull);
}

} // namespace

std::optional<float> busyPercent(std::uint64_t idle, std::uint64_t kernel, std::uint64_t user)
{
    std::uint64_t all = kernel + user;
    if (all == 0 || idle > all) return std::nullopt;
    return static_cast<float>(100.0 * static_cast<double>(all - idle) / static_cast<double>(all));
}

std::optional<GpuEngineInstance> parseGpuEngine(std::string_view instance)
{
    auto engineText = after(instance, "_eng_");
    auto adapter = parseAdapterLuid(instance);
    if (!engineText || !adapter || !after(instance, "pid_")) return std::nullopt;
    auto engine = number(*engineText);
    if (!engine) return std::nullopt;
    return GpuEngineInstance{ *adapter, static_cast<std::uint32_t>(*engine) };
}

std::vector<GpuLoad> gpuLoads(const std::vector<CounterValue>& engineInstances)
{
    std::map<std::uint64_t, std::map<std::uint32_t, double>> adapters; // by luid, then engine number: its processes together
    for (const CounterValue& c : engineInstances) {
        auto parsed = parseGpuEngine(c.instance);
        if (!parsed || !(c.value >= 0)) continue; // not a number: not a load
        adapters[parsed->adapter][parsed->engine] += c.value;
    }
    std::vector<GpuLoad> loads;
    for (const auto& [luid, engines] : adapters) {
        GpuLoad load{ .adapter = luid };
        for (const auto& [index, engine] : engines) load.load = std::max(load.load, engine);
        loads.push_back(load);
    }
    return loads;
}

float gpuLoadOf(const GpuAdapter& adapter, const std::vector<GpuLoad>& loads)
{
    for (const GpuLoad& l : loads)
        if (l.adapter == adapter.luid) return static_cast<float>(l.load);
    return 0.0f;
}

const GpuAdapter* chooseGpu(const std::vector<GpuAdapter>& adapters, const std::vector<GpuLoad>& loads)
{
    const GpuAdapter* chosen = nullptr;
    auto rank = [&](const GpuAdapter& a) { return std::pair{ gpuLoadOf(a, loads), a.dedicatedMemory }; };
    for (const GpuAdapter& a : adapters)
        if (!a.software && (!chosen || rank(a) > rank(*chosen))) chosen = &a;
    return chosen;
}

std::optional<float> hottestZone(const std::vector<CounterValue>& zones, double perKelvin)
{
    std::optional<float> hottest;
    for (const CounterValue& zone : zones) {
        if (!(zone.value > 0) || !(perKelvin > 0)) continue;
        float celsius = static_cast<float>(zone.value / perKelvin - 273.15);
        if (!hottest || celsius > *hottest) hottest = celsius;
    }
    return hottest;
}

} // namespace cup
