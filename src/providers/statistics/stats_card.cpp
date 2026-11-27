#include "providers/statistics/stats_card.h"

#include "core/stat_parts.h"
#include "core/tokens.h"

#include <algorithm>
#include <utility>

namespace cup {
namespace {

// One of the top row's tiles: a load big over its level.
StatTile loadTile(Glyph glyph, const char* caption, std::string detail, std::optional<float> percent, bool alert)
{
    return { .glyph = glyph, .caption = caption, .detail = std::move(detail), .value = percentText(percent),
             .level = percent.value_or(0) / 100, .alert = alert, .large = true };
}

} // namespace

bool hasGpu(const StatsReading& r) { return cleanPercent(r.gpuLoad).has_value(); }

StatTile cpuTile(const StatsReading& r)
{
    // Its temperature; on a PC that doesn't report one (most), how many cores the load is of.
    std::string detail = temperatureText(r.cpuTemperature);
    if (detail.empty() && r.cpuCores && *r.cpuCores > 0) detail = std::to_string(*r.cpuCores) + (*r.cpuCores == 1 ? " core" : " cores");
    return loadTile(Glyph::Chip, "CPU", std::move(detail), cleanPercent(r.cpuLoad), highLoad(r.cpuLoad) || highTemperature(r.cpuTemperature));
}

StatTile gpuTile(const StatsReading& r)
{
    return loadTile(Glyph::GraphicsCard, "GPU", temperatureText(r.gpuTemperature), cleanPercent(r.gpuLoad),
                    highLoad(r.gpuLoad) || highTemperature(r.gpuTemperature));
}

StatTile memoryTile(const StatsReading& r, bool roomy)
{
    auto percent = usedPercent(r.memoryUsed, r.memoryTotal);
    std::string detail;
    if (percent) detail = roomy ? usedOfText(r.memoryUsed, r.memoryTotal) : sizeText(std::min(*r.memoryUsed, *r.memoryTotal));
    return loadTile(Glyph::Memory, "RAM", std::move(detail), percent, highLoad(percent));
}

StatTile diskTile(const StatsReading& r)
{
    auto percent = usedPercent(r.diskUsed, r.diskTotal);
    return { .glyph = Glyph::Drive, .caption = "Disk", .value = percentText(percent), .level = percent.value_or(0) / 100, .alert = highLoad(percent) };
}

StatTile networkTile(const StatsReading& r)
{
    std::string speeds = speedPairText(r.networkDown, r.networkUp);
    // No name: both speeds with their unit take the line, and a name that came and went as they grew would flicker.
    return { .glyph = Glyph::Network, .value = speeds.empty() ? kNoReading : speeds };
}

BatteryTile bluetoothTile(const std::vector<BluetoothDevice>& devices)
{
    std::vector<BluetoothDevice> shown = shownDevices(devices);
    BatteryTile tile;
    if (shown.empty()) {
        tile.note = "No devices connected";
        return tile;
    }
    // More than fit: the last line says how many more, so the emptiest still show.
    std::size_t lines = batteryTileLines(), rows = shown.size();
    if (rows > lines) {
        rows = lines > 0 ? lines - 1 : 0;
        tile.note = "+" + std::to_string(shown.size() - rows) + " more";
    }
    for (std::size_t i = 0; i < rows; ++i)
        tile.rows.push_back({ glyphOf(shown[i].kind), shown[i].name, *shown[i].battery / 100, lowBattery(shown[i].battery), percentText(shown[i].battery) });
    return tile;
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

Content statisticsContent(const StatsView& v)
{
    const StatsReading& r = v.reading;
    Content c{ .label = "System", .card = CardLayout::Columns };
    c.keepOpen = true;      // nothing on it to click: a click on a tile doesn't close it
    c.noIdleTimeout = true; // watched while something else runs, like the timer (decision 062)
    bool gpu = hasGpu(r);
    Row top{ .even = true };
    top.items.push_back(cpuTile(r));
    if (gpu) top.items.push_back(gpuTile(r));
    top.items.push_back(memoryTile(r, !gpu));
    c.expandedCenter = { top };
    Row network = row({ networkTile(r) });
    network.tileGap = true; // as far under the disk as the columns are apart
    c.expandedLeading = { row({ diskTile(r) }), network };
    c.expandedTrailing = { row({ bluetoothTile(v.devices) }) };
    // The pill (decision 174): what was picked in Settings, where it has a reading.
    if (std::string line = pillText(r, v.devices, v.picks); !line.empty()) {
        c.compactLeading = { row({ Icon{ Glyph::Gauge } }, Align::Center) };
        // Its numbers bright among the names (decision 068); two readings fit, a long pair swims.
        c.compactCenter = { row({ Text{ .value = line, .marquee = true, .centred = true, .brightDigits = true } }) };
        c.minimal = { row({ Icon{ Glyph::Gauge } }, Align::Center) };
    }
    return c;
}

bool hasPill(const Content& content) { return !content.compactCenter.empty(); }

Panel statisticsPanel(const StatsView& v)
{
    return { .id = kStatisticsPanelId, .content = statisticsContent(v), .icon = Glyph::Gauge };
}

} // namespace cup
