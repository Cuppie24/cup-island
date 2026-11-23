#include "providers/statistics/stats_pill.h"

#include "core/tokens.h"
#include "providers/statistics/stats_format.h"

#include <algorithm>
#include <sstream>

namespace cup {
namespace {

namespace pick = stats_pick;

struct Reading {
    const char* id;
    const char* name; // Settings' switch
};
// In the card's order, which is the pill's.
const Reading kReadings[] = { { pick::kCpuLoad, "CPU load" },   { pick::kCpuTemperature, "CPU temperature" }, { pick::kGpuLoad, "GPU load" },
                              { pick::kMemory, "Memory" },      { pick::kDisk, "Disk space" },                { pick::kNetwork, "Network speed" } };

bool has(const std::vector<std::string>& list, const std::string& value) { return std::find(list.begin(), list.end(), value) != list.end(); }

bool isReading(const std::string& id)
{
    return std::any_of(std::begin(kReadings), std::end(kReadings), [&](const Reading& r) { return id == r.id; });
}

// A name fit for one line of a file and of Settings: no line breaks.
bool storable(const std::string& name) { return !name.empty() && name.find_first_of("\r\n") == std::string::npos; }

} // namespace

std::string devicePick(const std::string& name) { return pick::kDevice + name; }

bool isPicked(const StatsPicks& picks, const std::string& id) { return has(picks.picked, id); }

bool devicePicked(const StatsPicks& picks)
{
    return std::any_of(picks.picked.begin(), picks.picked.end(), [](const std::string& id) { return id.rfind(pick::kDevice, 0) == 0; });
}

bool togglePick(StatsPicks& picks, const std::string& id)
{
    if (auto it = std::find(picks.picked.begin(), picks.picked.end(), id); it != picks.picked.end()) {
        picks.picked.erase(it);
        return true;
    }
    bool device = id.rfind(pick::kDevice, 0) == 0 && has(picks.devices, id.substr(std::string(pick::kDevice).size()));
    if (!isReading(id) && !device) return false;
    picks.picked.push_back(id);
    // The pill has room for so many: the newest pick stays, the oldest gives way.
    std::size_t most = tokens().statsPillReadings;
    if (picks.picked.size() > most) picks.picked.erase(picks.picked.begin(), picks.picked.end() - static_cast<std::ptrdiff_t>(most));
    return true;
}

bool rememberDevices(StatsPicks& picks, const std::vector<BluetoothDevice>& listed)
{
    std::vector<std::string> before = picks.devices;
    for (const BluetoothDevice& d : shownDevices(listed)) {
        if (!storable(d.name)) continue;
        std::erase(picks.devices, d.name); // seen again: the latest
        picks.devices.push_back(d.name);
    }
    // Too many: the ones seen longest ago go, but never one that's picked.
    std::size_t most = tokens().statsKnownDevices;
    for (std::size_t i = 0; picks.devices.size() > most && i < picks.devices.size();) {
        if (isPicked(picks, devicePick(picks.devices[i]))) ++i;
        else picks.devices.erase(picks.devices.begin() + static_cast<std::ptrdiff_t>(i));
    }
    return picks.devices != before;
}

std::string savePicks(const StatsPicks& picks)
{
    std::string text;
    for (const std::string& name : picks.devices) text += "device " + name + "\n";
    for (const std::string& id : picks.picked) text += "pick " + id + "\n";
    return text;
}

StatsPicks loadPicks(const std::string& text)
{
    StatsPicks picks;
    std::istringstream lines(text);
    std::vector<std::string> picked;
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("device ", 0) == 0 && line.size() > 7 && !has(picks.devices, line.substr(7))) picks.devices.push_back(line.substr(7));
        else if (line.rfind("pick ", 0) == 0) picked.push_back(line.substr(5));
    }
    // Through the same rule as a click: only real readings and known devices, no more than fit.
    std::vector<std::string> taken; // an id twice in a damaged file counts once: it's a pick, not a flip
    for (const std::string& id : picked) {
        if (has(taken, id)) continue;
        taken.push_back(id);
        togglePick(picks, id);
    }
    return picks;
}

std::vector<StatsChoice> choicesOf(const StatsPicks& picks)
{
    std::vector<StatsChoice> choices;
    for (const Reading& r : kReadings) choices.push_back({ r.id, r.name, isPicked(picks, r.id), false });
    for (auto name = picks.devices.rbegin(); name != picks.devices.rend(); ++name)
        choices.push_back({ devicePick(*name), *name, isPicked(picks, devicePick(*name)), true });
    return choices;
}

std::string pillText(const StatsReading& r, const std::vector<BluetoothDevice>& devices, const StatsPicks& picks)
{
    std::vector<std::string> parts;
    auto load = cleanPercent(r.cpuLoad);
    std::string temperature = temperatureText(r.cpuTemperature);
    bool wantLoad = isPicked(picks, pick::kCpuLoad) && load, wantTemperature = isPicked(picks, pick::kCpuTemperature) && !temperature.empty();
    // Both of the CPU's under one name.
    if (wantLoad && wantTemperature) parts.push_back("CPU " + percentText(load) + " · " + temperature);
    else if (wantLoad) parts.push_back("CPU " + percentText(load));
    else if (wantTemperature) parts.push_back("CPU " + temperature);
    if (auto gpu = cleanPercent(r.gpuLoad); gpu && isPicked(picks, pick::kGpuLoad)) parts.push_back("GPU " + percentText(gpu));
    if (auto memory = usedPercent(r.memoryUsed, r.memoryTotal); memory && isPicked(picks, pick::kMemory)) parts.push_back("RAM " + percentText(memory));
    if (auto disk = usedPercent(r.diskUsed, r.diskTotal); disk && isPicked(picks, pick::kDisk)) parts.push_back("Disk " + percentText(disk));
    if (std::string speeds = speedPairText(r.networkDown, r.networkUp); !speeds.empty() && isPicked(picks, pick::kNetwork)) parts.push_back(speeds);
    for (const BluetoothDevice& d : shownDevices(devices))
        if (isPicked(picks, devicePick(d.name))) parts.push_back(shortName(d.name, tokens().statsPillNameLength) + " " + percentText(d.battery));
    std::string text;
    for (const std::string& part : parts) text += (text.empty() ? "" : " · ") + part;
    return text;
}

StatsRequest cardRequest(bool bluetooth)
{
    return { .cpuLoad = true, .cpuTemperature = true, .gpuLoad = true, .gpuTemperature = true, .memory = true, .disk = true, .network = true,
             .bluetooth = bluetooth };
}

StatsRequest pillRequest(const StatsPicks& picks, bool bluetooth)
{
    return { .cpuLoad = isPicked(picks, pick::kCpuLoad), .cpuTemperature = isPicked(picks, pick::kCpuTemperature),
             .gpuLoad = isPicked(picks, pick::kGpuLoad), .memory = isPicked(picks, pick::kMemory), .disk = isPicked(picks, pick::kDisk),
             .network = isPicked(picks, pick::kNetwork), .bluetooth = bluetooth && devicePicked(picks) };
}

} // namespace cup
