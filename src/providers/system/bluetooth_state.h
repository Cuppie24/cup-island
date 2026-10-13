#pragma once
// Which paired Bluetooth devices are connected (decision 039), from what
// Windows says about them, which isn't always the same thing:
//  - the paired-devices watcher's IsConnected: often not updated when a
//    device drops, and never when the radio is switched off;
//  - the device's own ConnectionStatus (BluetoothDevice / BluetoothLEDevice),
//    raised when it changes: right, once known;
//  - the Bluetooth radio's state: off, nothing is connected.
// Pure, so the rule is tested on Linux; the provider feeds readings in.

#include "providers/system/system_panels.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup {

class BluetoothState {
public:
    // The watcher found or updated a paired device. `name`: only when it says one.
    void listed(const std::string& id, const std::optional<std::string>& name, std::optional<bool> connected);
    void unpaired(const std::string& id);                    // gone from the paired list
    void status(const std::string& id, bool connected);      // the device's own ConnectionStatus
    void battery(const std::string& id, std::optional<int> level);
    void radio(bool on);                                     // the Bluetooth radio switched on or off

    bool connected(const std::string& id) const;
    // What the widget shows: the connected devices, in the order they connected.
    std::vector<BluetoothDevice> shown() const;

private:
    struct Device {
        std::string name;
        std::optional<bool> listedConnected; // the watcher's word
        std::optional<bool> ownStatus;       // the device's own, which wins
        std::optional<int> battery;
        std::uint64_t connectedOrder = 0;
    };
    void reorder(Device& device, bool wasConnected, bool nowConnected);
    bool isConnected(const Device& device) const;

    std::map<std::string, Device> devices_;
    bool radioOn_ = true; // until Windows says otherwise (no radio found: trust the rest)
    std::uint64_t nextOrder_ = 1;
};

} // namespace cup
