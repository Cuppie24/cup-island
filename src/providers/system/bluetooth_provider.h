#pragma once
// Bluetooth widget on Home (decision 009): the paired device that's
// connected, with its battery when it reports one. Event-driven, from what
// Windows offers (decision 039), with the rule in BluetoothState:
//  - a device watcher over paired Bluetooth devices (Classic and LE): names,
//    pairing, and its IsConnected, which often lags;
//  - each paired device's own ConnectionStatusChanged, which doesn't;
//  - the Bluetooth radio's StateChanged: off, nothing is connected;
//  - one watcher per connected device over its device nodes, for the
//    battery level Windows keeps there (the one Settings shows).
// Handlers run on background threads and post to the UI thread.

#include "core/provider.h"
#include "providers/system/bluetooth_state.h"
#include "providers/system/system_panels.h"

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace cup {

class BluetoothProvider : public Provider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit BluetoothProvider(PostToUi postToUi) : post_(std::move(postToUi)) {}

    const char* id() const override { return "system.bluetooth"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action&) override {}

private:
    using Watcher = winrt::Windows::Devices::Enumeration::DeviceWatcher;
    using Info = winrt::Windows::Devices::Enumeration::DeviceInformation;
    using Update = winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
    using Properties = winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Windows::Foundation::IInspectable>;
    using Classic = winrt::Windows::Devices::Bluetooth::BluetoothDevice;
    using Le = winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
    using Radio = winrt::Windows::Devices::Radios::Radio;

    // A paired device: its node group, and the object whose connection status we follow.
    struct Paired {
        std::wstring container; // groups the device's nodes; "" if unknown
        bool watched = false;   // its device object was asked for
        Classic classic{ nullptr };
        Le le{ nullptr };
        Classic::ConnectionStatusChanged_revoker classicChanged;
        Le::ConnectionStatusChanged_revoker leChanged;
    };
    struct BatteryWatch {
        Watcher watcher{ nullptr };
        std::map<std::wstring, int> levels; // per device node that reports one
        Watcher::Added_revoker added;
        Watcher::Updated_revoker updated;
        Watcher::Removed_revoker removed;
    };

    void endpointChanged(const std::wstring& id, const Properties& properties, const std::optional<std::string>& name);
    winrt::fire_and_forget followDevice(std::wstring id, bool lowEnergy, std::shared_ptr<bool> alive);
    winrt::fire_and_forget followRadio(std::shared_ptr<bool> alive);
    void watchBatteries(); // one battery watch per connected device, no more
    void batteryChanged(const std::wstring& container);
    void publish();
    void post(std::function<void()> work); // to the UI thread, dropped after stop()

    PostToUi post_;
    std::unique_ptr<BluetoothPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): late callbacks become no-ops
    BluetoothState state_;

    Watcher endpoints_{ nullptr };
    Watcher::Added_revoker endpointAdded_;
    Watcher::Updated_revoker endpointUpdated_;
    Watcher::Removed_revoker endpointRemoved_;
    std::map<std::wstring, Paired> paired_;          // by endpoint id
    std::map<std::wstring, BatteryWatch> batteries_; // by container id
    Radio radio_{ nullptr };
    Radio::StateChanged_revoker radioChanged_;
};

} // namespace cup
