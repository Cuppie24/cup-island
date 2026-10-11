#pragma once
// Home's static widgets (decision 009): battery and the connected Bluetooth
// device. The presenters turn plain readings into panels: no WinRT here, so
// every rule is tested on Linux; the Windows providers only read and feed
// readings in. They publish only on change, and remove the panel when
// there's nothing to show (a desktop with no battery, no device connected).

#include "core/provider.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

struct BatteryReading {
    int percent = 0;       // 0..100
    bool pluggedIn = false; // charging, or full on the charger
    bool operator==(const BatteryReading&) const = default;
};

struct BluetoothDevice {
    std::string id;             // stable per device
    std::string name;
    std::optional<int> battery; // 0..100, when the device reports it
    bool operator==(const BluetoothDevice&) const = default;
};

class BatteryPresenter {
public:
    explicit BatteryPresenter(Host& host) : host_(host) {}
    void show(const std::optional<BatteryReading>& reading); // nullopt = no battery
    static constexpr const char* kPanelId = "system.battery";

private:
    Host& host_;
    std::optional<BatteryReading> last_;
};

// Windows' light/dark mode (apps and system together), as a toggle tile.
class ThemePresenter {
public:
    explicit ThemePresenter(Host& host) : host_(host) {}
    void show(std::optional<bool> dark); // nullopt = can't tell (no such setting): no tile
    static constexpr const char* kPanelId = "system.theme";
    static constexpr const char* kToggle = "toggle"; // the tile's tap action

private:
    Host& host_;
    std::optional<bool> last_;
};

class BluetoothPresenter {
public:
    explicit BluetoothPresenter(Host& host) : host_(host) {}
    void show(const std::vector<BluetoothDevice>& connected); // in connection order
    static constexpr const char* kPanelId = "system.bluetooth";

private:
    Host& host_;
    std::optional<Panel> last_;
};

// Content, pure.
Panel batteryPanel(const BatteryReading& reading);
Panel themePanel(bool dark);
std::optional<Panel> bluetoothPanel(const std::vector<BluetoothDevice>& connected); // nullopt: none connected

} // namespace cup
