#pragma once
// Bluetooth's Home widget and card (decision 164), from plain readings: no
// WinRT here, so every rule is tested on Linux. The widget shows as much of
// each connected device as its two lines hold; the card lists them all with
// their batteries. A battery's level is its text's colour, never a surface's.
// Off, both say so (never a dimmed list: dimmed devices still read as
// connected), and the widget keeps its height: its lines are fixed.

#include "core/provider.h"
#include "providers/system/bluetooth_state.h"

#include <optional>
#include <vector>

namespace cup {

struct BluetoothView {
    BluetoothRadio radio = BluetoothRadio::Unknown;
    std::vector<BluetoothDevice> devices; // connected, in the order they connected
    bool operator==(const BluetoothView&) const = default;
};

namespace bluetooth_action {
inline constexpr const char* kToggle = "toggle"; // the switch: the radio on or off
} // namespace bluetooth_action

// A battery level's tone: batteryGoodLevel and batteryLowLevel split it (decision 164).
Tone batteryTone(int level);

// The widget and its card. No Bluetooth on this PC: none.
std::optional<Panel> bluetoothPanel(const BluetoothView& view);

// Publishes only on change, removes the widget with no Bluetooth.
class BluetoothPresenter {
public:
    explicit BluetoothPresenter(Host& host) : host_(host) {}
    void show(const BluetoothView& view);
    static constexpr const char* kPanelId = "system.bluetooth";

private:
    Host& host_;
    std::optional<std::optional<Panel>> last_; // outer empty: nothing shown yet
};

} // namespace cup
