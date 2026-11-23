#include "providers/statistics/stats_counters.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <tuple>
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

// What follows `key` in `text` up to the next '_' (or the end, with `toEnd`).
std::optional<std::string_view> after(std::string_view text, std::string_view key, bool toEnd = false)
{
    std::size_t at = text.find(key);
    if (at == std::string_view::npos) return std::nullopt;
    std::string_view rest = text.substr(at + key.size());
    return toEnd ? rest : rest.substr(0, rest.find('_'));
}

std::optional<std::uint64_t> hexNumber(std::string_view text)
{
    if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
    return number(text, 16);
}

bool sameName(std::string_view a, std::string_view b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

} // namespace

CpuLoads cpuLoads(const std::vector<CounterValue>& instances)
{
    CpuLoads loads;
    std::vector<std::tuple<std::uint64_t, std::uint64_t, float>> cores; // group, number, percent
    for (const CounterValue& c : instances) {
        if (c.instance == "_Total") {
            loads.total = static_cast<float>(c.value);
            continue;
        }
        std::string_view name = c.instance;
        std::size_t comma = name.find(',');
        auto group = comma == std::string_view::npos ? std::optional<std::uint64_t>{ 0 } : number(name.substr(0, comma));
        auto core = number(comma == std::string_view::npos ? name : name.substr(comma + 1));
        if (group && core) cores.emplace_back(*group, *core, static_cast<float>(c.value)); // "0,_Total": no number, left out
    }
    std::sort(cores.begin(), cores.end(), [](const auto& a, const auto& b) {
        return std::pair{ std::get<0>(a), std::get<1>(a) } < std::pair{ std::get<0>(b), std::get<1>(b) };
    });
    for (const auto& core : cores) loads.cores.push_back(std::get<2>(core));
    return loads;
}

std::optional<float> busyPercent(std::uint64_t idle, std::uint64_t kernel, std::uint64_t user)
{
    std::uint64_t all = kernel + user;
    if (all == 0 || idle > all) return std::nullopt;
    return static_cast<float>(100.0 * static_cast<double>(all - idle) / static_cast<double>(all));
}

std::optional<std::uint64_t> parseAdapterLuid(std::string_view instance)
{
    // "luid_0x<high>_0x<low>": two numbers after one key.
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

std::optional<GpuEngineInstance> parseGpuEngine(std::string_view instance)
{
    auto pidText = after(instance, "pid_");
    auto engineText = after(instance, "_eng_");
    auto type = after(instance, "engtype_", true); // to the end: "Compute_0" has a '_' of its own
    auto adapter = parseAdapterLuid(instance);
    if (!pidText || !engineText || !type || !adapter) return std::nullopt;
    auto pid = number(*pidText);
    auto engine = number(*engineText);
    if (!pid || !engine || type->empty()) return std::nullopt;
    return GpuEngineInstance{ static_cast<std::uint32_t>(*pid), *adapter, static_cast<std::uint32_t>(*engine), std::string(*type) };
}

std::string engineName(std::string_view type)
{
    std::string name;
    for (std::size_t i = 0; i < type.size(); ++i) {
        char c = type[i];
        // "VideoDecode": a capital after a small letter starts a word, written small.
        bool newWord = i > 0 && std::isupper(static_cast<unsigned char>(c)) && std::islower(static_cast<unsigned char>(type[i - 1]));
        if (newWord) name += ' ';
        if (c == '_') name += ' ';
        else name += newWord ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : c;
    }
    return name;
}

std::vector<GpuLoad> gpuLoads(const std::vector<CounterValue>& engineInstances)
{
    struct Engine {
        std::string type;
        double load = 0;
        std::map<std::uint32_t, double> processes;
    };
    std::map<std::uint64_t, std::map<std::uint32_t, Engine>> adapters; // by luid, then engine number
    for (const CounterValue& c : engineInstances) {
        auto parsed = parseGpuEngine(c.instance);
        if (!parsed || !(c.value >= 0)) continue; // not a number: not a load
        Engine& engine = adapters[parsed->adapter][parsed->engine];
        engine.type = parsed->type;
        engine.load += c.value;
        engine.processes[parsed->pid] += c.value;
    }
    std::vector<GpuLoad> loads;
    for (const auto& [luid, engines] : adapters) {
        GpuLoad load{ .adapter = luid };
        std::map<std::string, double> byType;
        std::map<std::uint32_t, double> byProcess;
        for (const auto& [index, engine] : engines) {
            load.load = std::max(load.load, engine.load);
            double& type = byType[engine.type];
            type = std::max(type, engine.load);
            for (const auto& [pid, value] : engine.processes) byProcess[pid] = std::max(byProcess[pid], value);
        }
        for (const auto& [type, value] : byType) load.engines.push_back({ engineName(type), value });
        for (const auto& [pid, value] : byProcess) load.processes.push_back({ pid, value });
        loads.push_back(std::move(load));
    }
    return loads;
}

void fillGpu(StatsReading& reading, const std::vector<GpuAdapter>& adapters, bool countersRead, const std::vector<GpuLoad>& loads,
             const std::vector<CounterValue>& memoryInstances, const std::vector<CounterValue>& processIds)
{
    auto loadOf = [&](const GpuAdapter& a) -> const GpuLoad* {
        for (const GpuLoad& l : loads)
            if (l.adapter == a.luid) return &l;
        return nullptr;
    };
    const GpuAdapter* chosen = nullptr;
    const GpuLoad* chosenLoad = nullptr;
    for (const GpuAdapter& a : adapters) {
        if (a.software) continue;
        const GpuLoad* load = loadOf(a);
        auto rank = [](const GpuAdapter& adapter, const GpuLoad* l) { return std::pair{ l ? l->load : 0.0, adapter.dedicatedMemory }; };
        if (chosen && rank(a, load) <= rank(*chosen, chosenLoad)) continue;
        chosen = &a;
        chosenLoad = load;
    }
    if (!chosen) return;
    reading.gpuName = chosen->name;
    if (countersRead) reading.gpuLoad = chosenLoad ? static_cast<float>(chosenLoad->load) : 0.0f;
    if (chosenLoad) {
        reading.gpuEngines = chosenLoad->engines;
        reading.gpuProcesses = namedGpuProcesses(chosenLoad->processes, processIds);
    }
    // Used and total: both or neither (stats_reading.h).
    for (const CounterValue& memory : memoryInstances) {
        if (parseAdapterLuid(memory.instance) != chosen->luid || !(memory.value >= 0) || chosen->dedicatedMemory == 0) continue;
        reading.gpuMemoryUsed = static_cast<std::uint64_t>(memory.value);
        reading.gpuMemoryTotal = chosen->dedicatedMemory;
    }
}

std::string processName(std::string_view instance)
{
    return std::string(instance.substr(0, instance.rfind('#')));
}

std::vector<NamedValue> byProcessName(const std::vector<CounterValue>& instances, double divisor)
{
    std::vector<NamedValue> apps; // few enough names to look through
    for (const CounterValue& c : instances) {
        std::string name = processName(c.instance);
        if (name.empty() || name == "_Total" || sameName(name, "Idle") || !(c.value >= 0) || !(divisor > 0)) continue;
        auto same = std::find_if(apps.begin(), apps.end(), [&](const NamedValue& app) { return app.name == name; });
        if (same == apps.end()) apps.push_back({ name, c.value / divisor });
        else same->value += c.value / divisor;
    }
    return apps;
}

std::vector<NamedValue> namedGpuProcesses(const std::vector<PidValue>& loads, const std::vector<CounterValue>& processIds)
{
    std::vector<CounterValue> named;
    for (const PidValue& load : loads) {
        if (load.pid == 0) continue; // the idle process' number: nobody's
        auto owner = std::find_if(processIds.begin(), processIds.end(),
                                  [&](const CounterValue& id) { return id.value == static_cast<double>(load.pid) && processName(id.instance) != "_Total"; });
        if (owner != processIds.end()) named.push_back({ owner->instance, load.value });
    }
    return byProcessName(named, 1);
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
