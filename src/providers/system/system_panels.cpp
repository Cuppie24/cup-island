#include "providers/system/system_panels.h"

#include <algorithm>

namespace cup {

// The mode it's in, the way Settings words it; a click switches (the tap action).
Panel themePanel(bool dark)
{
    const char* mode = dark ? "Dark" : "Light";
    Content content{
        .label = std::string(mode) + " mode",
        .tile = { row({ Icon{ dark ? Glyph::Moon : Glyph::Sun } }), row({ Text{ mode, TextStyle::Title } }) },
    };
    return { .id = ThemePresenter::kPanelId, .content = content, .tapAction = ThemePresenter::kToggle };
}

void ThemePresenter::show(std::optional<bool> dark)
{
    if (dark == last_) return;
    last_ = dark;
    if (dark) host_.publishPanel(themePanel(*dark));
    else host_.removePanel(kPanelId);
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

void BluetoothPresenter::show(const std::vector<BluetoothDevice>& connected)
{
    std::optional<Panel> panel = bluetoothPanel(connected);
    if (panel == last_) return;
    if (panel) host_.publishPanel(*panel);
    else host_.removePanel(kPanelId);
    last_ = std::move(panel);
}

} // namespace cup
