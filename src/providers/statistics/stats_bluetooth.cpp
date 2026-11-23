#include "providers/statistics/stats_bluetooth.h"

#include <algorithm>
#include <cctype>

namespace cup {
namespace {

std::string lower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool contains(const std::string& text, std::string_view part)
{
    return text.find(part) != std::string::npos;
}

// The device itself, not one of its services: "BTHENUM\DEV_<address>", "BTHLE\DEV_<address>".
bool isDeviceNode(const BluetoothNode& node)
{
    std::string id = lower(node.instanceId);
    return id.starts_with("bthenum\\dev_") || id.starts_with("bthle\\dev_");
}

// A service's node is named after its device: "WH-1000XM4 Hands-Free AG".
std::string withoutService(std::string name)
{
    for (std::string_view suffix : { " Hands-Free AG Audio", " Hands-Free AG", " Hands-Free", " Stereo", " Avrcp Transport" }) {
        if (name.size() > suffix.size() && name.ends_with(suffix)) return name.substr(0, name.size() - suffix.size());
    }
    return name;
}

struct Gathered {
    std::string container;
    BluetoothDevice device;
    bool named = false; // by its device node: nothing renames it
    bool connected = false, disconnected = false;
    std::optional<std::uint32_t> classOfDevice;
};

} // namespace

DeviceKind kindOfClass(std::uint32_t classOfDevice)
{
    std::uint32_t major = (classOfDevice >> 8) & 0x1F, minor = (classOfDevice >> 2) & 0x3F;
    if (major == 0x04) { // audio/video: headset, hands-free, headphones
        if (minor == 0x01 || minor == 0x02 || minor == 0x06) return DeviceKind::Headphones;
        return DeviceKind::Other;
    }
    if (major != 0x05) return DeviceKind::Other; // peripheral
    std::uint32_t what = minor & 0x0F;           // the low bits: what else it is
    if (what == 0x01 || what == 0x02) return DeviceKind::Controller; // joystick, gamepad
    if (what == 0x05 || what == 0x07) return DeviceKind::Pen;        // digitizer tablet, digital pen
    if (minor & 0x20) return DeviceKind::Mouse; // pointing device (a keyboard with a touchpad: a mouse to charge all the same)
    if (minor & 0x10) return DeviceKind::Keyboard;
    return DeviceKind::Other;
}

DeviceKind kindOfName(std::string_view name, DeviceKind known)
{
    std::string n = lower(name);
    if (contains(n, "buds") || contains(n, "airpods") || contains(n, "earphone")) return DeviceKind::Earbuds;
    if (known != DeviceKind::Other) return known;
    if (contains(n, "headphone") || contains(n, "headset")) return DeviceKind::Headphones;
    if (contains(n, "mouse") || contains(n, "trackball")) return DeviceKind::Mouse;
    if (contains(n, "keyboard")) return DeviceKind::Keyboard;
    if (contains(n, "controller") || contains(n, "gamepad")) return DeviceKind::Controller;
    if (n == "pen" || n.starts_with("pen ") || contains(n, " pen")) return DeviceKind::Pen; // a word of its own: not "Open…"
    return DeviceKind::Other;
}

std::vector<BluetoothDevice> bluetoothDevices(const std::vector<BluetoothNode>& nodes)
{
    std::vector<Gathered> gathered;
    for (const BluetoothNode& node : nodes) {
        auto same = node.container.empty() ? gathered.end()
                                           : std::find_if(gathered.begin(), gathered.end(), [&](const Gathered& g) { return g.container == node.container; });
        if (same == gathered.end()) same = gathered.insert(gathered.end(), Gathered{ .container = node.container });
        Gathered& g = *same;
        if (isDeviceNode(node) && !node.name.empty()) {
            g.device.name = node.name;
            g.named = true;
        } else if (!g.named) {
            std::string name = withoutService(node.name);
            if (!name.empty() && (g.device.name.empty() || name.size() < g.device.name.size())) g.device.name = name;
        }
        if (node.battery && !g.device.battery) g.device.battery = node.battery;
        if (node.connected) (*node.connected ? g.connected : g.disconnected) = true;
        if (node.classOfDevice) g.classOfDevice = node.classOfDevice;
    }
    std::vector<BluetoothDevice> devices;
    for (Gathered& g : gathered) {
        if (g.disconnected && !g.connected) continue;
        g.device.kind = kindOfName(g.device.name, g.classOfDevice ? kindOfClass(*g.classOfDevice) : DeviceKind::Other);
        devices.push_back(std::move(g.device));
    }
    return devices;
}

} // namespace cup
