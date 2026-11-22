#include "providers/system/bluetooth_state.h"

#include <algorithm>

namespace cup {

void BluetoothState::node(const BluetoothNode& r)
{
    Node& n = nodes_[r.id];
    if (!r.container.empty()) n.container = r.container;
    if (r.name) n.name = r.name;
    if (r.connected) n.connected = *r.connected;
    if (r.battery) n.battery = std::clamp(*r.battery, 0, 100);
    reorder();
}

void BluetoothState::nodeRemoved(const std::string& id)
{
    nodes_.erase(id);
    reorder();
}

void BluetoothState::radio(BluetoothRadio radio)
{
    radio_ = radio;
    reorder();
}

// Any of its nodes connected, and the radio not off: the radio switched off
// leaves the records saying "connected" for a while (decision 039).
bool BluetoothState::isConnected(const std::string& container) const
{
    if (container.empty() || radio_ == BluetoothRadio::Off) return false;
    return std::any_of(nodes_.begin(), nodes_.end(), [&](const auto& n) { return n.second.container == container && n.second.connected; });
}

void BluetoothState::reorder()
{
    for (auto it = connectedAt_.begin(); it != connectedAt_.end();)
        it = isConnected(it->first) ? std::next(it) : connectedAt_.erase(it);
    for (const auto& [id, n] : nodes_)
        if (!connectedAt_.count(n.container) && isConnected(n.container)) connectedAt_[n.container] = nextOrder_++;
}

std::vector<BluetoothDevice> BluetoothState::connected() const
{
    std::vector<std::pair<std::uint64_t, BluetoothDevice>> found;
    for (const auto& [container, order] : connectedAt_) {
        BluetoothDevice d{ .id = container };
        for (const auto& [id, n] : nodes_) {
            if (n.container != container) continue;
            if (d.name.empty() && n.name) d.name = *n.name;
            if (n.battery) d.battery = std::max(d.battery.value_or(0), *n.battery); // the best any node reports
        }
        if (d.name.empty()) d.name = "Bluetooth device";
        found.push_back({ order, d });
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<BluetoothDevice> out;
    for (auto& [order, d] : found) out.push_back(std::move(d));
    return out;
}

} // namespace cup
