#pragma once
// A reading's values made fit to show (decisions 171, 172), pure: bad sensor
// values cleaned, numbers rounded and given units, which count as high, and
// which rows of a list make it onto a page.

#include "providers/statistics/stats_reading.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// Shown where a reading is missing but its place is kept.
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
// "11.6%": a share in a list, one decimal under 100. Negative or not a number: "0%".
std::string shareText(double percent);
// "61°", to the nearest whole; "" without one. Cleans first.
std::string temperatureText(std::optional<float> celsius);
// "310 MB", "4.2 GB", "1.8 TB" (binary, as Task Manager counts): three
// digits at most, one decimal from GB up when it isn't ".0".
std::string sizeText(std::uint64_t bytes);
// "10.1 / 16 GB", "0.6 / 2 TB": both in the total's unit, the used never
// over the total; "" without both.
std::string usedOfText(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total);
// "148 GB free"; "" without both.
std::string freeText(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total);
// "340 KB/s", "1.2 MB/s", "1.1 GB/s": never more than three digits, one
// decimal under 10 from MB up; under 1 KB/s, "0 KB/s" (bytes would only
// flicker). "" without a reading. Cleans first.
std::string speedText(std::optional<float> bytesPerSecond);
// Both directions on one line, in the faster one's unit: "↓ 0.7 ↑ 0.3 MB/s",
// "↓ 340 ↑ 12 KB/s"; a missing one is kNoReading. "" without either.
std::string speedPairText(std::optional<float> down, std::optional<float> up);
// A link's speed, decimal as adapters are sold: "100 Mbps", "2.5 Gbps". "" for none or 0.
std::string linkText(std::optional<std::uint64_t> bitsPerSecond);

// At or over statsHighLoad / statsHighTemperature; at or under
// statsLowBattery. Judged on the number shown ("90%" is high, never "89%").
// No reading: not high, not low.
bool highLoad(std::optional<float> percent);
bool highTemperature(std::optional<float> celsius);
bool lowBattery(std::optional<float> percent);
// A direction at its link's limit: as high a share of the link's speed as a load is high.
bool atLinkLimit(std::optional<float> bytesPerSecond, std::optional<std::uint64_t> linkBitsPerSecond);

// A list for a page: entries with a name and a real, non-negative value,
// the biggest first (by name among equals, so the order holds from reading
// to reading), `most` at most.
std::vector<NamedValue> topValues(const std::vector<NamedValue>& values, std::size_t most);

// A Bluetooth device as shown: its level cleaned (its own, else its
// emptiest part's), its parts cleaned.
struct ShownDevices {
    std::vector<BluetoothDevice> devices;
    std::size_t more = 0; // ...and this many that didn't fit
    bool operator==(const ShownDevices&) const = default;
};
// Only devices with a name and a battery level, their own or a part's; the
// emptiest first (the one to charge is the one to see), by name among
// equals. `room` at most; the rest are counted.
ShownDevices shownDevices(const std::vector<BluetoothDevice>& devices, std::size_t room);
// "Headphones", "Mouse"; "Bluetooth device" for any other.
std::string kindName(DeviceKind kind);

} // namespace cup
