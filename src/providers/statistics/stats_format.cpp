#include "providers/statistics/stats_format.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cup {
namespace {

constexpr double kKilo = 1024; // binary units, as Task Manager counts

// `value` with one decimal, or none when it would be ".0" or `whole` says so.
std::string number(double value, bool whole)
{
    double tenths = std::round(value * 10) / 10;
    char text[32];
    if (whole || tenths == std::floor(tenths)) std::snprintf(text, sizeof text, "%.0f", std::round(whole ? value : tenths));
    else std::snprintf(text, sizeof text, "%.1f", tenths);
    return text;
}

std::string gigabytes(std::uint64_t bytes)
{
    double gb = static_cast<double>(bytes) / (kKilo * kKilo * kKilo);
    return number(gb, gb >= 99.95); // three digits at most: "128", not "128.3"
}

} // namespace

std::optional<float> cleanPercent(std::optional<float> percent)
{
    if (!percent || !std::isfinite(*percent)) return std::nullopt;
    return std::clamp(*percent, 0.0f, 100.0f);
}

std::optional<float> cleanTemperature(std::optional<float> celsius)
{
    if (!celsius || !std::isfinite(*celsius)) return std::nullopt;
    if (*celsius <= 0 || *celsius > static_cast<float>(tokens().statsPlausibleTemperature)) return std::nullopt;
    return celsius;
}

std::optional<float> usedPercent(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total)
{
    if (!used || !total || *total == 0) return std::nullopt;
    return cleanPercent(static_cast<float>(100.0 * static_cast<double>(*used) / static_cast<double>(*total)));
}

std::optional<float> cleanSpeed(std::optional<float> bytesPerSecond)
{
    if (!bytesPerSecond || !std::isfinite(*bytesPerSecond) || *bytesPerSecond < 0) return std::nullopt;
    return bytesPerSecond;
}

std::string percentText(std::optional<float> percent)
{
    auto clean = cleanPercent(percent);
    return clean ? std::to_string(std::lround(*clean)) + "%" : kNoReading;
}

std::string temperatureText(std::optional<float> celsius)
{
    auto clean = cleanTemperature(celsius);
    return clean ? std::to_string(std::lround(*clean)) + " °C" : "";
}

std::string memoryText(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total)
{
    if (!used || !total || *total == 0) return "";
    return gigabytes(std::min(*used, *total)) + " / " + gigabytes(*total) + " GB";
}

std::string speedText(std::optional<float> bytesPerSecond)
{
    auto clean = cleanSpeed(bytesPerSecond);
    if (!clean) return "";
    double value = *clean / kKilo;
    const char* units[] = { "KB/s", "MB/s", "GB/s" };
    std::size_t unit = 0;
    // The next unit once this one would round to four digits ("1000 KB/s").
    while (value >= 999.5 && unit < 2) value /= kKilo, ++unit;
    bool whole = unit == 0 || value >= 9.95; // "340 KB/s", "12 MB/s", "1.2 MB/s"
    return number(value, whole) + " " + units[unit];
}

bool highLoad(std::optional<float> percent)
{
    auto clean = cleanPercent(percent);
    return clean && std::lround(*clean) >= static_cast<long>(tokens().statsHighLoad); // as shown: "90%" is high, never "89%"
}

bool highTemperature(std::optional<float> celsius)
{
    auto clean = cleanTemperature(celsius);
    return clean && std::lround(*clean) >= static_cast<long>(tokens().statsHighTemperature);
}

bool lowBattery(std::optional<float> percent)
{
    auto clean = cleanPercent(percent);
    return clean && std::lround(*clean) <= static_cast<long>(tokens().statsLowBattery);
}

bool anythingHigh(const StatsReading& r)
{
    return highLoad(r.cpuLoad) || highLoad(r.gpuLoad) || highLoad(usedPercent(r.memoryUsed, r.memoryTotal)) ||
           highTemperature(r.cpuTemperature) || highTemperature(r.gpuTemperature);
}

std::string shortName(const std::string& name)
{
    std::size_t most = tokens().statsDeviceNameMaxLength, characters = 0, cut = 0; // cut: where the kept part would end
    for (std::size_t i = 0; i < name.size(); ++i) {
        bool starts = (static_cast<unsigned char>(name[i]) & 0xC0) != 0x80; // not a UTF-8 continuation byte
        if (!starts) continue;
        // Cut, it keeps one character less: the "…" takes a place.
        if (most > 0 && characters == most - 1) cut = i;
        if (characters == most) {
            std::string kept = name.substr(0, cut);
            while (!kept.empty() && kept.back() == ' ') kept.pop_back(); // no "Name …"
            return kept + "…";
        }
        ++characters;
    }
    return name;
}

ShownDevices shownDevices(const std::vector<BluetoothDevice>& devices, std::size_t room)
{
    ShownDevices shown;
    for (const BluetoothDevice& d : devices)
        if (auto battery = cleanPercent(d.battery); battery && !d.name.empty()) shown.devices.push_back({ d.name, battery });
    std::stable_sort(shown.devices.begin(), shown.devices.end(), [](const BluetoothDevice& a, const BluetoothDevice& b) {
        long levelA = std::lround(*a.battery), levelB = std::lround(*b.battery); // as shown, so a wobble under 1% doesn't reorder
        return levelA != levelB ? levelA < levelB : a.name < b.name;
    });
    if (shown.devices.size() > room) {
        std::size_t kept = room > 0 ? room - 1 : 0; // the last place says how many more
        shown.more = shown.devices.size() - kept;
        shown.devices.resize(kept);
    }
    return shown;
}

} // namespace cup
