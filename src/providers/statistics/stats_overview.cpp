#include "providers/statistics/stats_card.h"

#include "core/tokens.h"

#include <algorithm>
#include <utility>

namespace cup {
namespace {

namespace act = stats_action;

// One of the top row's tiles: a load's history, its percent big at the top right.
Graph loadTile(const char* caption, std::string detail, std::optional<float> percent, const StatsHistory::Samples& samples, bool alert,
               const char* action)
{
    Graph graph = historyGraph(percentSamples(samples), GraphSize::Tall);
    graph.tile = true;
    graph.caption = caption;
    graph.detail = std::move(detail);
    graph.value = percentText(percent);
    graph.valueLarge = true;
    graph.alert = alert;
    graph.actionId = action;
    return graph;
}

Graph cpuTile(const StatsView& v)
{
    const StatsReading& r = v.reading;
    return loadTile("CPU", temperatureText(r.cpuTemperature), cleanPercent(r.cpuLoad), samplesOf(v.history, Series::CpuLoad),
                    highLoad(r.cpuLoad) || highTemperature(r.cpuTemperature), act::kOpenCpu);
}

Graph gpuTile(const StatsView& v)
{
    const StatsReading& r = v.reading;
    return loadTile("GPU", temperatureText(r.gpuTemperature), cleanPercent(r.gpuLoad), samplesOf(v.history, Series::GpuLoad),
                    highLoad(r.gpuLoad) || highTemperature(r.gpuTemperature), act::kOpenGpu);
}

// `roomy`: two tiles on top, so there's room for the gigabytes in use beside the name.
Graph memoryTile(const StatsView& v, bool roomy)
{
    const StatsReading& r = v.reading;
    auto percent = usedPercent(r.memoryUsed, r.memoryTotal);
    std::string detail = roomy && percent ? sizeText(std::min(*r.memoryUsed, *r.memoryTotal)) : "";
    return loadTile("RAM", std::move(detail), percent, samplesOf(v.history, Series::Memory), highLoad(percent), act::kOpenMemory);
}

// Both speeds on one line over the download's history: the direction that's usually the bigger.
Graph networkTile(const StatsView& v)
{
    const StatsReading& r = v.reading;
    Graph graph = historyGraph(speedSamples(samplesOf(v.history, Series::Download)), GraphSize::Low);
    graph.tile = true;
    std::string speeds = speedPairText(r.networkDown, r.networkUp);
    graph.caption = speeds.empty() ? "Network " + std::string(kNoReading) : speeds;
    graph.captionCentred = true;
    graph.alert = atLinkLimit(r.networkDown, r.linkSpeed);
    graph.actionId = act::kOpenNetwork;
    return graph;
}

// The system drive: how full, and what's free.
LevelTile diskTile(const StatsReading& r)
{
    const DriveReading& drive = r.drives.front();
    auto percent = usedPercent(drive.used, drive.total);
    return { .glyph = Glyph::Drive, .caption = driveCaption(drive.label), .value = percentText(percent), .level = percent.value_or(0) / 100,
             .alert = highLoad(percent), .detail = freeText(drive.used, drive.total), .actionId = act::kOpenDisk };
}

// The devices with the least charge, as many as fit side by side. None: it says so, and still opens its page.
BatteryTile bluetoothTile(const StatsReading& r)
{
    BatteryTile tile{ .empty = "No devices", .actionId = act::kOpenBluetooth };
    for (const BluetoothDevice& d : shownDevices(r.bluetooth, tokens().statsBluetoothShown).devices)
        tile.cells.push_back({ glyphOf(d.kind), *d.battery / 100, lowBattery(d.battery), percentText(d.battery) });
    return tile;
}

// A row of tiles in equal columns.
Row tiles(std::vector<Item> items)
{
    return Row{ .items = std::move(items), .even = true };
}

} // namespace

Graph historyGraph(std::vector<float> samples, GraphSize size)
{
    return { .samples = std::move(samples), .capacity = tokens().statsHistoryLength, .size = size };
}

const char* openAction(StatsPage page)
{
    switch (page) {
    case StatsPage::Cpu: return act::kOpenCpu;
    case StatsPage::Gpu: return act::kOpenGpu;
    case StatsPage::Memory: return act::kOpenMemory;
    case StatsPage::Network: return act::kOpenNetwork;
    case StatsPage::Disk: return act::kOpenDisk;
    case StatsPage::Bluetooth: return act::kOpenBluetooth;
    case StatsPage::Overview: break;
    }
    return "";
}

std::optional<StatsPage> pageOpenedBy(const std::string& actionId)
{
    for (StatsPage page : { StatsPage::Cpu, StatsPage::Gpu, StatsPage::Memory, StatsPage::Network, StatsPage::Disk, StatsPage::Bluetooth })
        if (actionId == openAction(page)) return page;
    return std::nullopt;
}

bool hasPage(const StatsReading& r, StatsPage page)
{
    if (page == StatsPage::Gpu) return cleanPercent(r.gpuLoad).has_value();
    if (page == StatsPage::Disk) return !r.drives.empty();
    return true;
}

Glyph glyphOf(DeviceKind kind)
{
    switch (kind) {
    case DeviceKind::Headphones: return Glyph::Headphones;
    case DeviceKind::Earbuds: return Glyph::Earbuds;
    case DeviceKind::Mouse: return Glyph::Mouse;
    case DeviceKind::Keyboard: return Glyph::Keyboard;
    case DeviceKind::Controller: return Glyph::Controller;
    case DeviceKind::Pen: return Glyph::Pen;
    case DeviceKind::Other: break;
    }
    return Glyph::Bluetooth;
}

std::string driveCaption(const std::string& label)
{
    // "(C:)" at its end, as Explorer names a drive.
    std::size_t open = label.rfind('(');
    if (open != std::string::npos && open + 3 < label.size() && label[open + 2] == ':' && label[open + 3] == ')')
        return "Disk " + label.substr(open + 1, 2);
    return label.empty() ? "Disk" : label;
}

Slot overviewRows(const StatsView& v)
{
    bool gpu = hasPage(v.reading, StatsPage::Gpu);
    std::vector<Item> top = { cpuTile(v) };
    if (gpu) top.push_back(gpuTile(v));
    top.push_back(memoryTile(v, !gpu));
    std::vector<Item> bottom = { networkTile(v) };
    if (hasPage(v.reading, StatsPage::Disk)) bottom.push_back(diskTile(v.reading));
    bottom.push_back(bluetoothTile(v.reading));
    Row under = tiles(std::move(bottom));
    under.tileGap = true; // as far under the top row as the tiles are apart
    return { tiles(std::move(top)), under };
}

Content overviewContent(const StatsView& v)
{
    Content c{ .label = "System", .card = CardLayout::Centred };
    c.keepOpen = true;      // a click that just misses a tile doesn't close the card
    c.noIdleTimeout = true; // watched while something else runs, like the timer (decision 062)
    c.expandedCenter = overviewRows(v);
    return c;
}

Content statisticsContent(const StatsView& v)
{
    if (!hasPage(v.reading, v.page)) return overviewContent(v);
    switch (v.page) {
    case StatsPage::Cpu: return cpuPage(v);
    case StatsPage::Gpu: return gpuPage(v);
    case StatsPage::Memory: return memoryPage(v);
    case StatsPage::Network: return networkPage(v);
    case StatsPage::Disk: return diskPage(v);
    case StatsPage::Bluetooth: return bluetoothPage(v);
    case StatsPage::Overview: break;
    }
    return overviewContent(v);
}

Panel statisticsPanel(const StatsView& v)
{
    return { .id = kStatisticsPanelId, .content = statisticsContent(v), .icon = Glyph::Gauge };
}

} // namespace cup
