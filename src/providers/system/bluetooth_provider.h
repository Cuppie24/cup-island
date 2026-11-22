#pragma once
// Bluetooth on Home (decision 164): the connected devices with their
// batteries, and a switch for the radio. It reads only what Windows has
// already written down, never the Bluetooth stack:
//  - one watcher over Bluetooth's device nodes (DeviceInformationKind::Device,
//    Plug and Play's records: no radio traffic): their names, their "is
//    connected" record and the battery level Windows keeps on them;
//  - the Bluetooth radio's state (Windows.Devices.Radios), and setting it
//    when the user flips the switch.
// No association-endpoint watcher (it runs an inquiry over the air) and no
// BluetoothDevice / BluetoothLEDevice objects (opening one can bring up a
// link): either shares the radio's airtime with the music, and Bluetooth
// headphones stuttered (decision 042). Every rule is in BluetoothState and
// bluetoothPanel; handlers run on background threads and post to the UI thread.

#include "core/provider.h"
#include "providers/system/bluetooth_panel.h"

#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include <functional>
#include <memory>

namespace cup {

class BluetoothProvider : public Provider {
public:
    using PostToUi = std::function<void(std::function<void()>)>;
    explicit BluetoothProvider(PostToUi postToUi) : post_(std::move(postToUi)) {}

    const char* id() const override { return "system.bluetooth"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action& action) override; // the switch

private:
    using Watcher = winrt::Windows::Devices::Enumeration::DeviceWatcher;
    using Radio = winrt::Windows::Devices::Radios::Radio;

    void watchNodes();
    winrt::fire_and_forget followRadio(std::shared_ptr<bool> alive);
    winrt::fire_and_forget setRadio(bool on, std::shared_ptr<bool> alive);
    void publish();
    void post(std::function<void()> work); // to the UI thread, dropped after stop()

    PostToUi post_;
    std::unique_ptr<BluetoothPresenter> presenter_;
    std::shared_ptr<bool> alive_; // false after stop(): late callbacks become no-ops
    BluetoothState state_;
    bool listed_ = false;    // the watcher has reported every node once
    bool radioRead_ = false; // ...and the radio was read (or can't be): publish from then on

    Watcher nodes_{ nullptr };
    Watcher::Added_revoker added_;
    Watcher::Updated_revoker updated_;
    Watcher::Removed_revoker removed_;
    Watcher::EnumerationCompleted_revoker completed_;
    Radio radio_{ nullptr };
    Radio::StateChanged_revoker radioChanged_;
};

} // namespace cup
