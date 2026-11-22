#include "providers/system/bluetooth_panel.h"

#include "core/tokens.h"

namespace cup {
namespace {

Text percent(int level, TextStyle style)
{
    return Text{ .value = std::to_string(level) + "%", .style = style, .tone = batteryTone(level) };
}

bool canSwitch(BluetoothRadio radio) { return radio == BluetoothRadio::On || radio == BluetoothRadio::Off; }

// Two lines, whatever the count, so the widget never changes height:
// one device, its name over its battery; two, a line each, the battery at
// its end; three and more, how many over their batteries side by side
// (the names are on the card).
Slot widgetRows(const BluetoothView& v)
{
    const auto& d = v.devices;
    if (v.radio == BluetoothRadio::Off)
        return { row({ Text{ "Bluetooth is off", TextStyle::TileTitle } }), row({ Text{ "Turn on to see your devices", TextStyle::Body } }) };
    if (d.empty()) return { row({ Text{ "Bluetooth", TextStyle::TileTitle } }), row({ Text{ "No devices connected", TextStyle::Body } }) };
    if (d.size() == 1)
        return { row({ Text{ d[0].name, TextStyle::TileTitle } }),
                 row({ d[0].battery ? percent(*d[0].battery, TextStyle::Body) : Text{ "Connected", TextStyle::Body } }) };
    if (d.size() == 2) {
        Slot rows;
        for (const BluetoothDevice& device : d) {
            Row r = row({ Text{ device.name, TextStyle::Body } });
            if (device.battery) r.trailing.push_back(percent(*device.battery, TextStyle::Body));
            rows.push_back(r);
        }
        return rows;
    }
    Row levels;
    for (const BluetoothDevice& device : d)
        if (device.battery) levels.items.push_back(percent(*device.battery, TextStyle::Body));
    if (levels.items.empty()) levels.items.push_back(Text{ "Connected", TextStyle::Body });
    levels.hug = true; // each as wide as it is, side by side
    return { row({ Text{ std::to_string(d.size()) + " devices", TextStyle::TileTitle } }), levels };
}

// The card: the switch at the top, then every connected device, its name,
// its level as a gauge (the one place a battery's colour isn't text: the
// bar is a gauge, not a surface) and the number.
Slot cardRows(const BluetoothView& v)
{
    Row header = row({ Text{ "Bluetooth", TextStyle::Title } });
    if (canSwitch(v.radio)) header.trailing.push_back(Switch{ .on = v.radio == BluetoothRadio::On, .actionId = bluetooth_action::kToggle });
    Slot rows{ header };
    if (v.radio == BluetoothRadio::Off) {
        rows.push_back(row({ Text{ .value = "Bluetooth is off. Turn it on to see your devices.", .style = TextStyle::Caption, .centred = true } }));
        return rows;
    }
    if (v.devices.empty()) {
        rows.push_back(row({ Text{ .value = "No devices connected", .style = TextStyle::Caption, .centred = true } }));
        return rows;
    }
    for (const BluetoothDevice& d : v.devices) {
        Row r = row({ Icon{ Glyph::Bluetooth }, Text{ d.name, TextStyle::Body } });
        if (d.battery) {
            // A gauge, not a time: a still timeline as long as 100.
            r.items.push_back(Progress{ .timeline = { .position = Ms{ *d.battery }, .duration = Ms{ 100 } } });
            r.trailing.push_back(percent(*d.battery, TextStyle::Body));
        } else {
            r.trailing.push_back(Text{ "Connected", TextStyle::Caption });
        }
        rows.push_back(r);
    }
    return rows;
}

} // namespace

Tone batteryTone(int level)
{
    const Tokens& t = tokens();
    if (level > static_cast<int>(t.batteryGoodLevel)) return Tone::Good;
    if (level > static_cast<int>(t.batteryLowLevel)) return Tone::Fair;
    return Tone::Low;
}

std::optional<Panel> bluetoothPanel(const BluetoothView& v)
{
    if (v.radio == BluetoothRadio::Missing) return std::nullopt;
    Content c{ .label = "Bluetooth", .card = CardLayout::Centred };
    c.tileLeading = { row({ Icon{ Glyph::Bluetooth } }, Align::Center) };
    c.tile = widgetRows(v);
    // The switch is the widget's one control, in the middle of its column (decision 163).
    if (canSwitch(v.radio)) c.tileControls = { .primary = Switch{ .on = v.radio == BluetoothRadio::On, .actionId = bluetooth_action::kToggle } };
    c.expandedCenter = cardRows(v);
    return Panel{ .id = BluetoothPresenter::kPanelId, .content = c, .wide = true };
}

void BluetoothPresenter::show(const BluetoothView& view)
{
    std::optional<Panel> panel = bluetoothPanel(view);
    if (last_ && *last_ == panel) return;
    if (panel) host_.publishPanel(*panel);
    else if (last_ && *last_) host_.removePanel(kPanelId);
    last_ = std::move(panel);
}

} // namespace cup
