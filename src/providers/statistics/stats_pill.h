#pragma once
// What statistics shows in the collapsed pill (decision 174), pure: the
// readings the user picked in Settings, how a pick is made and kept, the
// line the pill reads, and what the reader is asked for with the card
// closed (only what's picked).

#include "providers/statistics/stats_reading.h"

#include <string>
#include <vector>

namespace cup {

// A pickable reading's id: what's saved, and what Settings sends back.
namespace stats_pick {
inline constexpr const char* kCpuLoad = "cpu";
inline constexpr const char* kCpuTemperature = "cputemp";
inline constexpr const char* kGpuLoad = "gpu";
inline constexpr const char* kMemory = "memory";
inline constexpr const char* kDisk = "disk";
inline constexpr const char* kNetwork = "network";
inline constexpr const char* kDevice = "bt:"; // + a Bluetooth device's name: its battery
} // namespace stats_pick

struct StatsPicks {
    std::vector<std::string> picked;  // ids, the one picked longest ago first; statsPillReadings at most
    std::vector<std::string> devices; // Bluetooth devices seen, by name, the latest seen last: Settings lists them
    bool operator==(const StatsPicks&) const = default;
};

std::string devicePick(const std::string& name); // a device's id
bool isPicked(const StatsPicks& picks, const std::string& id);
bool devicePicked(const StatsPicks& picks); // any device's battery
// Flips a pick. On: it joins the picks, and with more than statsPillReadings
// the one picked longest ago is dropped, so a switch always answers.
// False (nothing changed) for an id that isn't a reading or a known device.
bool togglePick(StatsPicks& picks, const std::string& id);
// Adds the devices of a listing to the known ones (those with a name and a
// level); true if the list changed. statsKnownDevices at most: the ones
// seen longest ago go first, never a picked one.
bool rememberDevices(StatsPicks& picks, const std::vector<BluetoothDevice>& listed);

// For Host::save and back: a line per pick ("pick cpu") and per device
// ("device WH-1000XM5"), in order. Anything else in the text is skipped.
std::string savePicks(const StatsPicks& picks);
StatsPicks loadPicks(const std::string& text);

// A switch of Settings' Statistics page.
struct StatsChoice {
    std::string id;
    std::string name; // "CPU load", "WH-1000XM5"
    bool on = false;
    bool device = false; // a Bluetooth device's battery: listed in its own group
    bool operator==(const StatsChoice&) const = default;
};
// Every reading that can be picked, then every known device, the latest seen first.
std::vector<StatsChoice> choicesOf(const StatsPicks& picks);

// The pill's line: each picked reading that has a value, in the card's
// order, " · " between them ("CPU 14% · 46°", "RAM 52% · Buds 74%"). A pick
// with no reading (no temperature on this PC, a device not connected) is
// left out; "" when none has one, and then there is no pill.
// `devices`: the last listing.
std::string pillText(const StatsReading& reading, const std::vector<BluetoothDevice>& devices, const StatsPicks& picks);

// What to read with the card shown: everything on it. `bluetooth`: the devices are due.
StatsRequest cardRequest(bool bluetooth);
// ...and with it closed: only what's picked.
StatsRequest pillRequest(const StatsPicks& picks, bool bluetooth);

} // namespace cup
