#include "providers/statistics/stats_card.h"

#include "core/tokens.h"

#include <cmath>
#include <utility>

namespace cup {
namespace {

// A row of equal columns that lines up with the tiles' (the same gap in each).
Row columns(std::vector<Item> items)
{
    return Row{ .items = std::move(items), .align = Align::Center, .loose = true, .even = true };
}

Text cell(std::string value, TextStyle style)
{
    return Text{ .value = std::move(value), .style = style, .centred = true };
}

// A line of names and values: the values (its digits) bright among quiet words (decision 068).
Text quietCell(std::string value)
{
    Text text = cell(std::move(value), TextStyle::Caption);
    text.brightDigits = true;
    return text;
}

} // namespace

StatTile cpuTile(const StatsReading& r)
{
    return { "CPU", cleanPercent(r.cpuLoad), temperatureText(r.cpuTemperature) };
}

StatTile gpuTile(const StatsReading& r)
{
    return { "GPU", cleanPercent(r.gpuLoad), temperatureText(r.gpuTemperature) };
}

StatTile memoryTile(const StatsReading& r)
{
    return { "Memory", usedPercent(r.memoryUsed, r.memoryTotal), memoryText(r.memoryUsed, r.memoryTotal) };
}

// A Progress fills from a Timeline (content.h): one that isn't playing,
// `percent` of the way along, is a bar that stands at that level.
Progress levelBar(std::optional<float> percent)
{
    if (!percent) return {}; // no duration: an empty track
    return { .timeline = { .position = Ms{ std::lround(*percent * 10) }, .duration = Ms{ 1000 } } };
}

// The details always get their row, even all empty: a temperature that
// comes and goes mustn't move the rows around it.
Slot tileRows(const std::vector<StatTile>& tiles)
{
    std::vector<Item> names, values, bars, details;
    for (const StatTile& tile : tiles) {
        names.push_back(cell(tile.label, TextStyle::Caption));
        values.push_back(cell(percentText(tile.percent), TextStyle::CardTitle)); // what the card is about (design rules)
        bars.push_back(levelBar(tile.percent));
        details.push_back(quietCell(tile.detail));
    }
    return { columns(std::move(names)), columns(std::move(values)), columns(std::move(bars)), columns(std::move(details)) };
}

std::vector<std::string> otherTexts(const StatsReading& r)
{
    std::vector<std::string> texts;
    if (auto disk = usedPercent(r.diskUsed, r.diskTotal)) texts.push_back("Disk " + percentText(disk));
    if (std::string down = speedText(r.networkDown); !down.empty()) texts.push_back("↓ " + down);
    if (std::string up = speedText(r.networkUp); !up.empty()) texts.push_back("↑ " + up);
    if (auto battery = cleanPercent(r.battery)) texts.push_back((r.charging ? "Charging " : "Battery ") + percentText(battery));
    return texts;
}

std::optional<Row> otherRow(const StatsReading& r)
{
    std::vector<Item> items;
    for (std::string& text : otherTexts(r)) items.push_back(quietCell(std::move(text)));
    if (items.empty()) return std::nullopt;
    Row row = columns(std::move(items));
    row.apart = true; // a group of its own under the tiles (decision 066)
    return row;
}

std::optional<Row> bluetoothRow(const StatsReading& r)
{
    ShownDevices shown = shownDevices(r.bluetooth, tokens().statsBluetoothShown);
    std::vector<Item> items;
    for (const BluetoothDevice& device : shown.devices) {
        // Only the level bright, not every digit: a name has its own ("WH-1000XM4").
        std::string name = shortName(device.name) + " ", level = percentText(device.battery);
        Text text = cell(name + level, TextStyle::Caption);
        text.bright = { { name.size(), level.size() } };
        items.push_back(std::move(text));
    }
    if (shown.more > 0) items.push_back(cell("+" + std::to_string(shown.more) + " more", TextStyle::Caption));
    if (items.empty()) return std::nullopt;
    Row row = columns(std::move(items));
    row.apart = true;
    return row;
}

Content statisticsContent(const StatsReading& r)
{
    Content c{ .label = "System", .card = CardLayout::Centred };
    // Something high: every fill in the danger colour. An accent is the
    // card's, not a bar's (decision 010), so which one is high is read from
    // its number (decision 171).
    if (anythingHigh(r)) c.accent = tokens().dangerColor;
    c.noIdleTimeout = true; // watched while something else runs, like the timer (decision 062)
    c.expandedCenter = tileRows({ cpuTile(r), gpuTile(r), memoryTile(r) });
    if (auto other = otherRow(r)) c.expandedCenter.push_back(std::move(*other));
    if (auto bluetooth = bluetoothRow(r)) c.expandedCenter.push_back(std::move(*bluetooth));
    return c;
}

Panel statisticsPanel(const StatsReading& r)
{
    return { .id = kStatisticsPanelId, .content = statisticsContent(r), .icon = Glyph::Gauge };
}

} // namespace cup
