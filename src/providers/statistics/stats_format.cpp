#include "providers/statistics/stats_format.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cup {
namespace {

constexpr double kKilo = 1024; // binary units, as Task Manager counts

// `value` with at most `decimals` decimals, none of them a trailing zero ("16", "10.1", "1.31").
std::string number(double value, int decimals)
{
    char text[32];
    std::snprintf(text, sizeof text, "%.*f", decimals, value);
    std::string s = text;
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

// A size's unit: 0 KB, 1 MB, 2 GB, 3 TB. The next one once this one would show four digits.
const char* const kSizeUnits[] = { "KB", "MB", "GB", "TB" };
int sizeUnit(std::uint64_t bytes)
{
    double value = static_cast<double>(bytes) / kKilo;
    int unit = 0;
    while (value >= 999.5 && unit < 3) value /= kKilo, ++unit;
    return unit;
}
// `bytes` in that unit: whole under GB, a decimal in GB under 100, two in TB under 10.
std::string inUnit(std::uint64_t bytes, int unit)
{
    double value = static_cast<double>(bytes) / std::pow(kKilo, unit + 1);
    int decimals = unit == 3 && value < 9.995 ? 2 : unit >= 2 && value < 99.95 ? 1 : 0;
    return number(value, decimals);
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

std::string shareText(double percent)
{
    if (!std::isfinite(percent) || percent < 0) percent = 0;
    return number(std::min(percent, 100.0), percent < 99.95 ? 1 : 0) + "%";
}

std::string temperatureText(std::optional<float> celsius)
{
    auto clean = cleanTemperature(celsius);
    return clean ? std::to_string(std::lround(*clean)) + "°" : "";
}

std::string sizeText(std::uint64_t bytes)
{
    int unit = sizeUnit(bytes);
    return inUnit(bytes, unit) + " " + kSizeUnits[unit];
}

std::string usedOfText(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total)
{
    if (!used || !total || *total == 0) return "";
    int unit = sizeUnit(*total);
    return inUnit(std::min(*used, *total), unit) + " / " + inUnit(*total, unit) + " " + kSizeUnits[unit];
}

std::string freeText(std::optional<std::uint64_t> used, std::optional<std::uint64_t> total)
{
    if (!used || !total || *total == 0) return "";
    return sizeText(*total - std::min(*used, *total)) + " free";
}

namespace {

const char* const kSpeedUnits[] = { "KB/s", "MB/s", "GB/s" };
// The unit a speed reads in: the next one once this one would round to four digits ("1000 KB/s").
int speedUnit(float bytesPerSecond)
{
    double value = bytesPerSecond / kKilo;
    int unit = 0;
    while (value >= 999.5 && unit < 2) value /= kKilo, ++unit;
    return unit;
}
// ...and its number in a unit: whole in KB and from 10 up ("340", "12"), else a decimal ("1.2").
std::string speedIn(float bytesPerSecond, int unit)
{
    double value = bytesPerSecond / std::pow(kKilo, unit + 1);
    return number(value, unit == 0 || value >= 9.95 ? 0 : 1);
}

} // namespace

std::string speedText(std::optional<float> bytesPerSecond)
{
    auto clean = cleanSpeed(bytesPerSecond);
    if (!clean) return "";
    int unit = speedUnit(*clean);
    return speedIn(*clean, unit) + " " + kSpeedUnits[unit];
}

std::string speedPairText(std::optional<float> down, std::optional<float> up)
{
    auto d = cleanSpeed(down), u = cleanSpeed(up);
    if (!d && !u) return "";
    int unit = speedUnit(std::max(d.value_or(0), u.value_or(0)));
    return "\u2193 " + (d ? speedIn(*d, unit) : kNoReading) + " \u2191 " + (u ? speedIn(*u, unit) : kNoReading) + " " + kSpeedUnits[unit];
}

std::string linkText(std::optional<std::uint64_t> bitsPerSecond)
{
    if (!bitsPerSecond || *bitsPerSecond == 0) return "";
    double mega = static_cast<double>(*bitsPerSecond) / 1e6;
    if (mega >= 999.5) return number(mega / 1000, mega < 9995 ? 1 : 0) + " Gbps";
    return number(mega, mega < 9.95 ? 1 : 0) + " Mbps";
}

bool highLoad(std::optional<float> percent)
{
    auto clean = cleanPercent(percent);
    return clean && std::lround(*clean) >= static_cast<long>(tokens().statsHighLoad);
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

bool atLinkLimit(std::optional<float> bytesPerSecond, std::optional<std::uint64_t> link)
{
    auto speed = cleanSpeed(bytesPerSecond);
    if (!speed || !link || *link == 0) return false;
    return highLoad(static_cast<float>(100.0 * 8.0 * *speed / static_cast<double>(*link)));
}

std::vector<NamedValue> topValues(const std::vector<NamedValue>& values, std::size_t most)
{
    std::vector<NamedValue> top;
    for (const NamedValue& v : values)
        if (!v.name.empty() && std::isfinite(v.value) && v.value >= 0) top.push_back(v);
    std::stable_sort(top.begin(), top.end(), [](const NamedValue& a, const NamedValue& b) {
        return a.value != b.value ? a.value > b.value : a.name < b.name;
    });
    if (top.size() > most) top.resize(most);
    return top;
}

ShownDevices shownDevices(const std::vector<BluetoothDevice>& devices, std::size_t room)
{
    ShownDevices shown;
    for (const BluetoothDevice& d : devices) {
        BluetoothDevice clean{ d.name, cleanPercent(d.battery), d.kind, {} };
        std::optional<float> emptiest;
        for (const NamedValue& part : d.parts)
            if (auto level = cleanPercent(static_cast<float>(part.value)); level && !part.name.empty()) {
                clean.parts.push_back({ part.name, *level });
                emptiest = emptiest ? std::min(*emptiest, *level) : *level;
            }
        if (!clean.battery) clean.battery = emptiest; // earbuds that only report their parts
        if (clean.battery && !clean.name.empty()) shown.devices.push_back(std::move(clean));
    }
    std::stable_sort(shown.devices.begin(), shown.devices.end(), [](const BluetoothDevice& a, const BluetoothDevice& b) {
        long levelA = std::lround(*a.battery), levelB = std::lround(*b.battery); // as shown, so a wobble under 1% doesn't reorder
        return levelA != levelB ? levelA < levelB : a.name < b.name;
    });
    if (shown.devices.size() > room) {
        shown.more = shown.devices.size() - room;
        shown.devices.resize(room);
    }
    return shown;
}

std::string kindName(DeviceKind kind)
{
    switch (kind) {
    case DeviceKind::Headphones: return "Headphones";
    case DeviceKind::Earbuds: return "Earbuds";
    case DeviceKind::Mouse: return "Mouse";
    case DeviceKind::Keyboard: return "Keyboard";
    case DeviceKind::Controller: return "Controller";
    case DeviceKind::Pen: return "Pen";
    case DeviceKind::Other: break;
    }
    return "Bluetooth device";
}

} // namespace cup
