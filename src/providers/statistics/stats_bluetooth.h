#pragma once
// Bluetooth devices from Windows' device tree (decision 173), pure. The
// reader lists the tree's Bluetooth nodes once, reading only what Windows
// already keeps about them (no watcher, no radio: decision 042), and hands
// them here: a device is several nodes (itself, and one per service such as
// "Hands-Free"), and its battery level sits on one of them.

#include "providers/statistics/stats_reading.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

// One node of the device tree under a Bluetooth enumerator.
struct BluetoothNode {
    std::string container;                      // which device it's part of (its container id); "": its own
    std::string instanceId;                     // "BTHENUM\DEV_AC80..." (the device) or "BTHENUM\{0000111E-...}..." (a service)
    std::string name;                           // "WH-1000XM4", "WH-1000XM4 Hands-Free AG"
    std::optional<float> battery;               // percent, where Windows keeps one
    std::optional<bool> connected;              // where Windows says
    std::optional<std::uint32_t> classOfDevice; // what the device says it is (Bluetooth's class of device)
    bool operator==(const BluetoothNode&) const = default;
};

// The devices those nodes are: a container's nodes together. Named after
// its device node (else its shortest name, without a service's suffix);
// the level is whichever node has one. A device Windows says is
// disconnected is left out: its level is the last one heard, maybe days
// old. One without a level stays (shownDevices drops it).
std::vector<BluetoothDevice> bluetoothDevices(const std::vector<BluetoothNode>& nodes);

// What a class of device says it is: headphones and headsets, mice,
// keyboards, game controllers, pens; Other for the rest.
DeviceKind kindOfClass(std::uint32_t classOfDevice);
// A guess from the name where the class doesn't say (Bluetooth LE devices
// have none) or can't (earbuds call themselves headsets): "…Buds", "AirPods", "…Mouse".
DeviceKind kindOfName(std::string_view name, DeviceKind known);

} // namespace cup
