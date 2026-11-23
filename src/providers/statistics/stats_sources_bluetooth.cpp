// Bluetooth devices' battery levels from the device tree (SetupAPI,
// Windows Vista+ for device properties; decision 173).
//
// Decision 042 removed Bluetooth because a standing device watcher kept the
// radio busy and music stuttered. So this is one listing per call, of what
// the PnP manager already keeps in memory about the nodes under the
// Bluetooth enumerators: no DeviceWatcher, no notification, no Bluetooth or
// radio API, no device opened, nothing sent over the air. The set is
// destroyed before returning: nothing stays open between calls.
#include "providers/statistics/stats_sources.h"

#include "platform/win.h"

#include <devpropdef.h>
#include <setupapi.h>

#include <cstring>

namespace cup::stats {
namespace {

// Written out rather than taken from devpkey.h: two of them aren't in it.
// The battery level Settings shows for a Bluetooth device: a byte, percent.
constexpr DEVPROPKEY kBattery{ { 0x104EA319, 0x6EE2, 0x4701, { 0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5 } }, 2 };
// Whether the device is connected: a boolean, on the device's own node. Not documented; absent, the device is taken as connected.
constexpr DEVPROPKEY kConnected{ { 0x83DA6326, 0x97A6, 0x4088, { 0x94, 0x53, 0xA1, 0x92, 0x3F, 0x57, 0x3B, 0x29 } }, 15 };
// DEVPKEY_Bluetooth_ClassOfDevice (bthdef.h): a 32-bit number.
constexpr DEVPROPKEY kClassOfDevice{ { 0x2BD67D8B, 0x8BEB, 0x48D5, { 0x87, 0xE0, 0x6C, 0xDA, 0x34, 0x28, 0x04, 0x0A } }, 10 };
// DEVPKEY_Device_ContainerId: the physical device a node belongs to, a GUID.
constexpr DEVPROPKEY kContainer{ { 0x8C7ED206, 0x3F8A, 0x4827, { 0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C } }, 2 };
// DEVPKEY_Device_FriendlyName, and DEVPKEY_NAME where a node has none.
constexpr DEVPROPKEY kFriendlyName{ { 0xA45C254E, 0xDF1C, 0x4EFD, { 0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0 } }, 14 };
constexpr DEVPROPKEY kName{ { 0xB725F130, 0x47EF, 0x101A, { 0xA5, 0xF1, 0x02, 0x60, 0x8C, 0x9E, 0xEB, 0xAC } }, 10 };

// A property's bytes if it's there and of `type`; `size` bytes at most.
bool property(HDEVINFO devices, SP_DEVINFO_DATA& device, const DEVPROPKEY& key, DEVPROPTYPE type, void* value, DWORD size)
{
    DEVPROPTYPE found = 0;
    DWORD needed = 0;
    return SetupDiGetDevicePropertyW(devices, &device, &key, &found, static_cast<PBYTE>(value), size, &needed, 0) && found == type;
}

std::string text(HDEVINFO devices, SP_DEVINFO_DATA& device, const DEVPROPKEY& key)
{
    wchar_t value[256] = {};
    if (!property(devices, device, key, DEVPROP_TYPE_STRING, value, sizeof(value) - sizeof(wchar_t))) return {};
    return utf8(value);
}

BluetoothNode nodeOf(HDEVINFO devices, SP_DEVINFO_DATA& device)
{
    BluetoothNode node;
    wchar_t id[512] = {};
    if (SetupDiGetDeviceInstanceIdW(devices, &device, id, 511, nullptr)) node.instanceId = utf8(id);
    node.name = text(devices, device, kFriendlyName);
    if (node.name.empty()) node.name = text(devices, device, kName);

    GUID container{};
    // All ones in its last eight bytes: Windows' "no container", not a device to gather by.
    const GUID none{ 0, 0, 0, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };
    if (property(devices, device, kContainer, DEVPROP_TYPE_GUID, &container, sizeof(container)) && std::memcmp(&container, &none, sizeof(GUID)) != 0) {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&container);
        static const char kHex[] = "0123456789abcdef";
        for (std::size_t i = 0; i < sizeof(GUID); ++i) {
            node.container += kHex[bytes[i] >> 4];
            node.container += kHex[bytes[i] & 0x0F];
        }
    }
    BYTE battery = 0;
    if (property(devices, device, kBattery, DEVPROP_TYPE_BYTE, &battery, sizeof(battery))) node.battery = static_cast<float>(battery);
    DEVPROP_BOOLEAN connected = DEVPROP_FALSE;
    if (property(devices, device, kConnected, DEVPROP_TYPE_BOOLEAN, &connected, sizeof(connected))) node.connected = connected != DEVPROP_FALSE;
    ULONG classOfDevice = 0;
    if (property(devices, device, kClassOfDevice, DEVPROP_TYPE_UINT32, &classOfDevice, sizeof(classOfDevice))) node.classOfDevice = classOfDevice;
    return node;
}

} // namespace

std::vector<BluetoothNode> bluetoothNodes()
{
    std::vector<BluetoothNode> nodes;
    // Classic devices and their services, Bluetooth LE devices, and theirs.
    for (const wchar_t* enumerator : { L"BTHENUM", L"BTHLE", L"BTHLEDEVICE" }) {
        HDEVINFO devices = SetupDiGetClassDevsW(nullptr, enumerator, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (devices == INVALID_HANDLE_VALUE) continue; // no such enumerator: no Bluetooth of that kind
        SP_DEVINFO_DATA device{};
        device.cbSize = sizeof(device);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(devices, i, &device); ++i) nodes.push_back(nodeOf(devices, device));
        SetupDiDestroyDeviceInfoList(devices);
    }
    return nodes;
}

} // namespace cup::stats
