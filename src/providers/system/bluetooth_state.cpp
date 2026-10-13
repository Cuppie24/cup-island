#include "providers/system/bluetooth_state.h"

#include <algorithm>

namespace cup {

bool BluetoothState::isConnected(const Device& d) const
{
    if (!radioOn_) return false; // the radio is off: whatever the lists still say
    if (d.ownStatus) return *d.ownStatus;
    return d.listedConnected.value_or(false);
}

void BluetoothState::reorder(Device& d, bool was, bool now)
{
    if (now && !was) d.connectedOrder = nextOrder_++; // newest last
}

void BluetoothState::listed(const std::string& id, const std::optional<std::string>& name, std::optional<bool> connected)
{
    Device& d = devices_[id];
    bool was = isConnected(d);
    if (name) d.name = *name;
    if (connected) d.listedConnected = *connected;
    reorder(d, was, isConnected(d));
}

void BluetoothState::unpaired(const std::string& id) { devices_.erase(id); }

void BluetoothState::status(const std::string& id, bool connected)
{
    auto it = devices_.find(id);
    if (it == devices_.end()) return; // not (or no longer) paired
    bool was = isConnected(it->second);
    it->second.ownStatus = connected;
    reorder(it->second, was, isConnected(it->second));
}

void BluetoothState::battery(const std::string& id, std::optional<int> level)
{
    if (auto it = devices_.find(id); it != devices_.end()) it->second.battery = level;
}

void BluetoothState::radio(bool on)
{
    if (on == radioOn_) return;
    std::map<std::string, bool> was;
    for (const auto& [id, d] : devices_) was[id] = isConnected(d);
    radioOn_ = on;
    for (auto& [id, d] : devices_) reorder(d, was[id], isConnected(d));
}

bool BluetoothState::connected(const std::string& id) const
{
    auto it = devices_.find(id);
    return it != devices_.end() && isConnected(it->second);
}

std::vector<BluetoothDevice> BluetoothState::shown() const
{
    std::vector<std::pair<std::uint64_t, BluetoothDevice>> on;
    for (const auto& [id, d] : devices_)
        if (isConnected(d)) on.push_back({ d.connectedOrder, { id, d.name, d.battery } });
    std::sort(on.begin(), on.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<BluetoothDevice> devices;
    for (auto& [order, d] : on) devices.push_back(std::move(d));
    return devices;
}

} // namespace cup
