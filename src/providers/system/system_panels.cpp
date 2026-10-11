#include "providers/system/system_panels.h"

#include <algorithm>

namespace cup {

Panel batteryPanel(const BatteryReading& r)
{
    int percent = std::clamp(r.percent, 0, 100);
    Content content{
        .label = "Battery " + std::to_string(percent) + "%",
        .tile = { row({ Icon{ r.pluggedIn ? Glyph::Charging : Glyph::Battery } }),
                  row({ Text{ std::to_string(percent) + "%", TextStyle::Title } }) },
    };
    return { .id = BatteryPresenter::kPanelId, .content = content };
}

// The device to show: one that reports its battery (headphones, a mouse)
// before one that doesn't, else the first connected. The rest are counted.
std::optional<Panel> bluetoothPanel(const std::vector<BluetoothDevice>& connected)
{
    if (connected.empty()) return std::nullopt;
    auto shown = std::find_if(connected.begin(), connected.end(), [](const BluetoothDevice& d) { return d.battery.has_value(); });
    if (shown == connected.end()) shown = connected.begin();

    Row status = shown->battery ? row({ Text{ std::to_string(std::clamp(*shown->battery, 0, 100)) + "%", TextStyle::Title } })
                                : row({ Text{ "Connected", TextStyle::Caption } });
    if (connected.size() > 1)
        status.items.push_back(Text{ "+" + std::to_string(connected.size() - 1) + " more", TextStyle::Caption, /* alignEnd */ true });
    Content content{
        .label = shown->name,
        .tile = { row({ Icon{ Glyph::Bluetooth }, Text{ shown->name } }), status },
    };
    return Panel{ .id = BluetoothPresenter::kPanelId, .content = content, .wide = true };
}

void BatteryPresenter::show(const std::optional<BatteryReading>& reading)
{
    if (reading == last_) return; // also: no battery, never shown, nothing to remove
    last_ = reading;
    if (reading) host_.publishPanel(batteryPanel(*reading));
    else host_.removePanel(kPanelId);
}

void BluetoothPresenter::show(const std::vector<BluetoothDevice>& connected)
{
    std::optional<Panel> panel = bluetoothPanel(connected);
    if (panel == last_) return;
    if (panel) host_.publishPanel(*panel);
    else host_.removePanel(kPanelId);
    last_ = std::move(panel);
}

} // namespace cup
