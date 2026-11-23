// The pages' shared pieces, and the two at the card's usual width: network and Bluetooth (decision 172).

#include "providers/statistics/stats_card.h"

#include "core/tokens.h"

#include <utility>

namespace cup {
namespace {

// A speed's history as a tile: its direction at the left, the speed now at the right.
Graph speedTile(const char* caption, std::optional<float> speed, const StatsHistory::Samples& samples, bool alert)
{
    Graph graph = historyGraph(speedSamples(samples), GraphSize::Tall);
    graph.tile = true;
    graph.caption = caption;
    std::string now = speedText(speed);
    graph.value = now.empty() ? kNoReading : now;
    graph.alert = alert;
    return graph;
}

// A table of facts: each value over what it is. Only the facts there are.
void factRows(Slot& rows, const std::vector<std::pair<std::string, std::string>>& facts)
{
    Row values{ .align = Align::Center, .even = true }, names = values;
    for (const auto& [value, name] : facts) {
        if (value.empty()) continue;
        values.items.push_back(Text{ .value = value, .style = TextStyle::Title, .centred = true, .brightDigits = true });
        names.items.push_back(Text{ .value = name, .style = TextStyle::Caption, .centred = true });
    }
    if (values.items.empty()) return;
    rows.push_back(std::move(values));
    rows.push_back(std::move(names));
}

// A device: its glyph, its name over its kind (or that it's low), its battery and percent.
Row deviceRow(const BluetoothDevice& d)
{
    bool low = lowBattery(d.battery);
    return Row{ .items = { Text{ .value = d.name, .style = TextStyle::Body, .detail = low ? "Low battery" : kindName(d.kind), .detailTrimsEnd = true } },
                .leading = { Icon{ glyphOf(d.kind) } },
                .trailing = { Battery{ .level = *d.battery / 100, .alert = low }, Text{ .value = percentText(d.battery), .style = TextStyle::Title,
                                                                                         .alignEnd = true, .sizedAs = "100%" } } };
}

// Under an earbuds' row: left, right and case side by side, each with its own battery.
Row partsRow(const BluetoothDevice& d)
{
    Row row{ .align = Align::Center, .even = true };
    for (const NamedValue& part : d.parts) {
        auto level = static_cast<float>(part.value);
        row.items.push_back(Battery{ .level = level / 100, .alert = lowBattery(level), .label = part.name, .value = percentText(level) });
    }
    return row;
}

} // namespace

Row pageHeader(const std::string& title, std::vector<Item> trailing)
{
    // Hugging and centred: the title in the card's middle whatever sits at the row's ends.
    return Row{ .items = { Text{ .value = title, .style = TextStyle::Title } },
                .align = Align::Center,
                .leading = { Button{ .glyph = Glyph::ChevronLeft, .actionId = stats_action::kBack, .compact = true } },
                .trailing = std::move(trailing),
                .hug = true };
}

Content pageContent(const StatsView& v, const std::string& label)
{
    Content c{ .label = label, .card = CardLayout::Centred };
    c.keepOpen = true;
    c.noIdleTimeout = true;
    c.backAction = stats_action::kBack; // Esc steps back to the overview (decision 033)
    c.sizedLike = overviewRows(v);      // the overview's height: opening a page doesn't make the card taller (decision 070)
    return c;
}

Row listRow(const std::string& name, float level, const std::string& value, const std::string& widest)
{
    return Row{ .items = { Text{ .value = name, .style = TextStyle::Caption } },
                .trailing = { Progress{ .level = level, .stub = true },
                              Text{ .value = value, .style = TextStyle::Caption, .alignEnd = true, .brightDigits = true, .sizedAs = widest } } };
}

// Download and upload as two graphs, each on its own scale (upload is often a tenth of download), and today's totals.
Content networkPage(const StatsView& v)
{
    const StatsReading& r = v.reading;
    Content c = pageContent(v, "Network");
    c.pageAtTop = true; // the header where every page's is, whatever is under it
    // How it's online: the Wi-Fi's name, or that it's a cable.
    std::string name = r.networkName.empty() && r.wired ? "Ethernet" : r.networkName;
    std::vector<Item> trailing;
    if (!name.empty()) trailing = { Icon{ r.wired ? Glyph::Ethernet : Glyph::Wifi }, Text{ .value = name, .style = TextStyle::Caption } };
    c.expandedCenter = { pageHeader("Network", std::move(trailing)),
                         Row{ .items = { speedTile("↓ Download", r.networkDown, samplesOf(v.history, Series::Download), atLinkLimit(r.networkDown, r.linkSpeed)),
                                         speedTile("↑ Upload", r.networkUp, samplesOf(v.history, Series::Upload), atLinkLimit(r.networkUp, r.linkSpeed)) },
                              .even = true } };
    factRows(c.expandedCenter, { { r.networkDownToday ? sizeText(*r.networkDownToday) : "", "Down today" },
                                 { r.networkUpToday ? sizeText(*r.networkUpToday) : "", "Up today" },
                                 { linkText(r.linkSpeed), "Link speed" } });
    return c;
}

// Every device with a battery level, the emptiest first, in a list that scrolls when they don't fit.
Content bluetoothPage(const StatsView& v)
{
    Content c = pageContent(v, "Bluetooth");
    c.pageAtTop = true; // the header at the top, the list filling what's under it
    ShownDevices shown = shownDevices(v.reading.bluetooth, v.reading.bluetooth.size());
    if (shown.devices.empty()) { // in the words and shape of media's "Nothing playing"
        Row icon = row({ Icon{ .glyph = Glyph::Bluetooth, .large = true } }, Align::Center);
        icon.apart = true;
        c.expandedCenter = { pageHeader("Bluetooth"), icon,
                             row({ Text{ .value = "No devices connected", .style = TextStyle::Title, .centred = true } }),
                             row({ Text{ .value = "Bluetooth devices with a battery show here", .style = TextStyle::Caption, .centred = true } }) };
        return c;
    }
    c.expandedCenter = { pageHeader("Bluetooth", { Text{ .value = std::to_string(shown.devices.size()) + " connected", .style = TextStyle::Caption,
                                                         .brightDigits = true } }) };
    for (const BluetoothDevice& d : shown.devices) {
        c.expandedCenter.push_back(deviceRow(d));
        if (!d.parts.empty()) c.expandedCenter.push_back(partsRow(d));
    }
    for (std::size_t i = 1; i < c.expandedCenter.size(); ++i) {
        c.expandedCenter[i].list = "devices";
        if (i == 1) c.expandedCenter[i].visibleRows = -1; // fills the card under the header (decision 070)
    }
    return c;
}

} // namespace cup
