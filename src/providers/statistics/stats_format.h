#pragma once
// A reading's values made fit to show (decision 171), pure: bad sensor
// values cleaned, numbers rounded and given units, which count as high, and
// which Bluetooth devices make it onto the card.

#include "providers/statistics/stats_reading.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// Shown where a reading is missing: the piece keeps its place, so the card
// doesn't rearrange as a sensor comes and goes.
inline constexpr const char* kNoReading = "—"; // —

// A percent as it can be shown: NaN and infinities are no reading; below 0
// is 0, above 100 is 100 (counters overshoot a little between samples).
std::optional<float> cleanPercent(std::optional<float> percent);
// A temperature in °C, or none. Never clamped: a made-up 125 °C would read
// as real. Not a number, 0 or below (what an unread sensor reports), or past
// statsPlausibleTemperature: no reading.
std::optional<float> cleanTemperature(std::optional<float> celsius);
// Used as a percent of total; none without both, or with a total of 0.
std::optional<float> usedPercent(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total);
// A speed in bytes per second: NaN, infinities and negatives are no reading.
std::optional<float> cleanSpeed(std::optional<float> bytesPerSecond);

// "42%", to the nearest whole; kNoReading without one. Cleans first.
std::string percentText(std::optional<float> percent);
// "61 °C", to the nearest whole; "" without one (it's a detail under a
// load: nothing to hold a place for). Cleans first.
std::string temperatureText(std::optional<float> celsius);
// "10.1 / 16 GB" (binary gigabytes, as Task Manager counts): one decimal
// under 100, none when it's .0; "" without both.
std::string memoryText(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total);
// "340 KB/s", "1.2 MB/s", "1.1 GB/s": never more than three digits, one
// decimal under 10 from MB up; under 1 KB/s, "0 KB/s" (bytes would only
// flicker). "" without a reading. Cleans first.
std::string speedText(std::optional<float> bytesPerSecond);

// At or over statsHighLoad / statsHighTemperature; at or under
// statsLowBattery. No reading: not high, not low.
bool highLoad(std::optional<float> percent);
bool highTemperature(std::optional<float> celsius);
bool lowBattery(std::optional<float> percent);
// Anything on the card high: a load, memory, a temperature. (Not the disk: a full drive is no emergency of the moment.)
bool anythingHigh(const StatsReading& reading);

// A device's name at most statsDeviceNameMaxLength characters (not bytes:
// never cut inside a UTF-8 character), ending in "…" when cut.
std::string shortName(const std::string& name);

// The Bluetooth devices the card shows, and how many it left out.
struct ShownDevices {
    std::vector<BluetoothDevice> devices; // battery cleaned, name as it came
    std::size_t more = 0;                 // ...and this many not shown ("+2 more")
    bool operator==(const ShownDevices&) const = default;
};
// Only devices with a battery level and a name; the emptiest first (the one
// to charge is the one to see), by name among equals, so the order holds
// from reading to reading. More than `room` (statsBluetoothShown): one place
// goes to the "+N more", the rest to the emptiest.
ShownDevices shownDevices(const std::vector<BluetoothDevice>& devices, std::size_t room);

} // namespace cup
