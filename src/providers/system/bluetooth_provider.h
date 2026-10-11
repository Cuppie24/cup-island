#pragma once
// Bluetooth widget on Home (decision 009): the paired device that's
// connected, with its battery when it reports one. Event-driven, through
// device watchers:
//  - one over paired Bluetooth devices (Classic and LE), for connections;
//  - one per connected device over its own device nodes, for the battery
//    level Windows keeps there (the one Settings shows). Not every device
//    reports it; those show "Connected".
// Handlers run on background threads and post to the UI thread; every
// decision is in BluetoothPresenter.

#include "core/provider.h"
#include "providers/system/system_panels.h"

#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include <cstdint>
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

    struct Endpoint {
        std::string name;
        std::wstring container; // groups the device's nodes; "" if unknown
        bool connected = false;
        std::uint64_t connectedOrder = 0;
    };
    struct BatteryWatch {
        Watcher watcher{ nullptr };
        std::map<std::wstring, int> levels; // per device node that reports one
        Watcher::Added_revoker added;
        Watcher::Updated_revoker updated;
        Watcher::Removed_revoker removed;
    };

    void endpointChanged(const std::wstring& id, const Properties& properties, const std::string* name);
    void watchBatteries();  // one battery watch per connected device, no more
    void publish();
    void post(std::function<void()> work); // to the UI thread, dropped after stop()

    PostToUi post_;
    std::unique_ptr<BluetoothPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): late callbacks become no-ops

    Watcher endpoints_{ nullptr };
    Watcher::Added_revoker endpointAdded_;
    Watcher::Updated_revoker endpointUpdated_;
    Watcher::Removed_revoker endpointRemoved_;
    std::map<std::wstring, Endpoint> devices_;           // by endpoint id
    std::map<std::wstring, BatteryWatch> batteries_;     // by container id
    std::uint64_t nextOrder_ = 1;
};

} // namespace cup
