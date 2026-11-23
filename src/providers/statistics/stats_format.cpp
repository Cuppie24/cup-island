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

std::string speedPairText(std::optional<float> down, std::optional<float> up)
{
    auto d = cleanSpeed(down), u = cleanSpeed(up);
    if (!d && !u) return "";
    int unit = speedUnit(std::max(d.value_or(0), u.value_or(0)));
    return "\u2193 " + (d ? speedIn(*d, unit) : kNoReading) + " \u2191 " + (u ? speedIn(*u, unit) : kNoReading) + " " + kSpeedUnits[unit];
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

std::vector<BluetoothDevice> shownDevices(const std::vector<BluetoothDevice>& devices)
{
    std::vector<BluetoothDevice> shown;
    for (const BluetoothDevice& d : devices)
        if (auto battery = cleanPercent(d.battery); battery && !d.name.empty()) shown.push_back({ d.name, battery, d.kind });
    std::stable_sort(shown.begin(), shown.end(), [](const BluetoothDevice& a, const BluetoothDevice& b) {
        long levelA = std::lround(*a.battery), levelB = std::lround(*b.battery); // as shown, so a wobble under 1% doesn't reorder
        return levelA != levelB ? levelA < levelB : a.name < b.name;
    });
    return shown;
}

std::string shortName(const std::string& name, std::size_t most)
{
    std::size_t characters = 0, cut = 0; // cut: where the kept part would end
    for (std::size_t i = 0; i < name.size(); ++i) {
        bool starts = (static_cast<unsigned char>(name[i]) & 0xC0) != 0x80; // not a UTF-8 continuation byte
        if (!starts) continue;
        if (most > 0 && characters == most - 1) cut = i; // cut, it keeps one character less: the "…" takes a place
        if (characters == most) {
            std::string kept = name.substr(0, cut);
            while (!kept.empty() && kept.back() == ' ') kept.pop_back(); // no "Name …"
            return kept + "\u2026";
        }
        ++characters;
    }
    return name;
}

} // namespace cup
