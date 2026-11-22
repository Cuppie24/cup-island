#pragma once
// Which Bluetooth devices are connected (decision 164), from Windows' own
// device records only: the device nodes Windows keeps for each paired
// device, and the radio. Nothing here, or in the provider that feeds it,
// asks the Bluetooth stack or the radio anything: a running inquiry or an
// opened device object shares the radio's airtime with the music, and Bluetooth
// headphones stuttered (decision 042). Pure, so the rule is tested on Linux.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// One device node, as Windows' device records say: a paired device has a
// few (the device, its services), grouped by their container. An update
// carries only what changed.
struct BluetoothNode {
    std::string id;                   // the node's instance id
    std::string container;            // groups one device's nodes; "" if not said (an update)
    std::optional<std::string> name;  // the device's name: from its own node, not a service's
    std::optional<bool> connected;    // the node's "is connected" record
    std::optional<int> battery;       // the level Windows keeps (the one Settings shows), 0..100
};

enum class BluetoothRadio { Unknown, On, Off, Missing }; // Missing: no Bluetooth on this PC

struct BluetoothDevice {
    std::string id; // its container
    std::string name;
    std::optional<int> battery;
    bool operator==(const BluetoothDevice&) const = default;
};

class BluetoothState {
public:
    void node(const BluetoothNode& reading); // added or updated
    void nodeRemoved(const std::string& id);
    void radio(BluetoothRadio radio);

    BluetoothRadio radio() const { return radio_; }
    // The connected devices, in the order they connected. The radio off: none.
    std::vector<BluetoothDevice> connected() const;

private:
    struct Node {
        std::string container;
        std::optional<std::string> name;
        bool connected = false;
        std::optional<int> battery;
    };
    bool isConnected(const std::string& container) const;
    void reorder(); // a device that just connected goes last

    std::map<std::string, Node> nodes_;               // by node id
    std::map<std::string, std::uint64_t> connectedAt_; // container -> order it connected in
    BluetoothRadio radio_ = BluetoothRadio::Unknown;
    std::uint64_t nextOrder_ = 1;
};

} // namespace cup
