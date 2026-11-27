// Statistics, the numbers (decisions 171, 174): cleaning and formatting a
// reading, which devices show, the pill's picks and its line, and what is
// asked of the reader. The card is in statistics_card_tests.cpp, the
// provider in statistics_provider_tests.cpp.

#include "core/tokens.h"
#include "expect.h"
#include "providers/statistics/stats_format.h"
#include "providers/statistics/stats_pill.h"
#include "statistics_fixture.h"

#include <cmath>
#include <limits>

namespace cup::test {
void runStatisticsCardTests();
void runStatisticsProviderTests();
namespace {

constexpr std::uint64_t kTb = 1024 * kGb;
const float kNan = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

void cleaningTests()
{
    expect(cleanPercent(42.4f) == 42.4f && !cleanPercent(std::nullopt), "a percent passes; none stays none");
    expect(!cleanPercent(kNan) && !cleanPercent(kInf) && !cleanPercent(-kInf), "not a number: no reading");
    expect(cleanPercent(-3.0f) == 0.0f && cleanPercent(104.0f) == 100.0f, "clamped to 0..100");
    expect(percentText(42.4f) == "42%" && percentText(42.5f) == "43%" && percentText(99.6f) == "100%" && percentText(0.2f) == "0%",
           "to the nearest whole");
    expect(percentText(std::nullopt) == kNoReading && percentText(kNan) == kNoReading && percentText(250.0f) == "100%", "none: a dash");

    expect(temperatureText(61.2f) == "61°" && temperatureText(61.5f) == "62°", "a temperature, whole degrees");
    expect(temperatureText(std::nullopt).empty() && temperatureText(kNan).empty(), "none: nothing");
    expect(temperatureText(0.0f).empty() && temperatureText(-273.15f).empty(), "an unread sensor's 0 or below: nothing");
    expect(temperatureText(static_cast<float>(tokens().statsPlausibleTemperature) + 1).empty() &&
           !temperatureText(static_cast<float>(tokens().statsPlausibleTemperature)).empty(), "past plausible: nothing, never clamped");

    expect(usedPercent(8 * kGb, 16 * kGb) == 50.0f, "used of total");
    expect(!usedPercent(8 * kGb, std::nullopt) && !usedPercent(std::nullopt, 16 * kGb) && !usedPercent(1, 0), "needs both, and a total");
    expect(usedPercent(20 * kGb, 16 * kGb) == 100.0f, "more used than there is: full");

    expect(sizeText(0) == "0 KB" && sizeText(310 * kMb) == "310 MB" && sizeText(kGb * 42 / 10) == "4.2 GB" && sizeText(16 * kGb) == "16 GB" &&
           sizeText(364 * kGb) == "364 GB", "a size: whole under GB, a decimal in GB");
    expect(sizeText(999 * kMb) == "999 MB" && sizeText(1000 * kMb) == "1 GB", "never four digits: the next unit");
    expect(sizeText(kTb * 131 / 100) == "1.31 TB" && sizeText(2 * kTb) == "2 TB", "terabytes: two decimals");
    expect(usedOfText(kGb * 87 / 10, 16 * kGb) == "8.7 / 16 GB" && usedOfText(364 * kGb, 512 * kGb) == "364 / 512 GB", "used of total, one unit");
    expect(usedOfText(20 * kGb, 16 * kGb) == "16 / 16 GB" && usedOfText(std::nullopt, 16 * kGb).empty() && usedOfText(1, 0).empty(),
           "used capped at the total; nothing without both");

    expect(speedPairText(9.9f * kMb, 0.3f * kMb) == "↓ 9.9 ↑ 0.3 MB/s" && speedPairText(340.0f * 1024, 12.0f * 1024) == "↓ 340 ↑ 12 KB/s",
           "both directions, one unit");
    expect(speedPairText(12.4f * kMb, 100.0f * 1024) == "↓ 12 ↑ 0.1 MB/s" && speedPairText(0.0f, 0.0f) == "↓ 0 ↑ 0 KB/s", "...the faster one's");
    expect(speedPairText(999.6f * 1024, 0.0f) == "↓ 1 ↑ 0 MB/s", "never four digits: the next unit");
    expect(speedPairText(std::nullopt, 2.0f * kMb) == std::string("↓ ") + kNoReading + " ↑ 2 MB/s" && speedPairText(kNan, std::nullopt).empty() &&
           speedPairText(-1.0f, kInf).empty(), "one missing: a dash; both, or garbage: nothing");
}

void levelTests()
{
    const Tokens& t = tokens();
    float high = static_cast<float>(t.statsHighLoad), hot = static_cast<float>(t.statsHighTemperature), low = static_cast<float>(t.statsLowBattery);
    expect(highLoad(high) && highLoad(100.0f) && highLoad(400.0f) && !highLoad(high - 1), "high from the threshold up");
    expect(highLoad(high - 0.4f) && !highLoad(high - 0.6f), "judged as shown: what reads as the threshold is high");
    expect(!highLoad(std::nullopt) && !highLoad(kNan), "no reading is never high");
    expect(highTemperature(hot) && !highTemperature(hot - 1) && !highTemperature(std::nullopt) && !highTemperature(4000.0f),
           "a hot temperature; a broken sensor's isn't");
    expect(lowBattery(low) && lowBattery(0.0f) && !lowBattery(low + 1) && !lowBattery(std::nullopt), "low down to the threshold");
}

void deviceTests()
{
    std::vector<BluetoothDevice> devices = { { "Keyboard", 90.0f, DeviceKind::Keyboard }, { "Mouse", 15.2f, DeviceKind::Mouse }, { "Speaker", std::nullopt },
                                             { "", 50.0f }, { "Buds", 15.4f }, { "Pen", kNan, DeviceKind::Pen }, { "Headset", 140.0f } };
    auto shown = shownDevices(devices);
    expect(shown.size() == 4, "only named devices with a battery level");
    expect(shown[0].name == "Buds" && shown[1].name == "Mouse" && shown[2].name == "Keyboard" && shown[3].name == "Headset",
           "the emptiest first; by name where they read the same");
    expect(shown[3].battery == 100.0f && shown[1].kind == DeviceKind::Mouse && shownDevices({}).empty(), "its level cleaned, its kind kept");

    expect(shortName("AirPods Pro", 12) == "AirPods Pro" && shortName("123456789012", 12) == "123456789012", "a name that fits is left alone");
    expect(shortName("Galaxy Buds2 Pro", 12) == "Galaxy Buds…" && shortName("WH-1000XM5 x", 11) == "WH-1000XM5…", "too long: cut, no space before the \"…\"");
    std::string accented;
    for (int i = 0; i < 14; ++i) accented += "é"; // two bytes each
    expect(shortName(accented, 12).size() == 2 * 11 + 3 && shortName("abc", 0) == "…", "counted in characters, never cut inside one");
}

void pickTests()
{
    const Tokens& t = tokens();
    namespace pick = stats_pick;
    StatsPicks picks;
    expect(togglePick(picks, pick::kCpuLoad) && isPicked(picks, pick::kCpuLoad) && picks.picked.size() == 1, "a switch on: picked");
    expect(togglePick(picks, pick::kCpuLoad) && picks.picked.empty(), "...and off again");
    expect(!togglePick(picks, "nonsense") && !togglePick(picks, "") && !togglePick(picks, devicePick("Unknown")) && picks.picked.empty(),
           "only a reading or a known device can be picked");
    // More than the pill holds: the one picked longest ago gives way.
    togglePick(picks, pick::kCpuLoad), togglePick(picks, pick::kMemory);
    expect(t.statsPillReadings == 2 && picks.picked == std::vector<std::string>{ pick::kCpuLoad, pick::kMemory }, "two fit, in the order picked");
    togglePick(picks, pick::kDisk);
    expect(picks.picked == std::vector<std::string>{ pick::kMemory, pick::kDisk }, "a third turns off the one picked longest ago");
    togglePick(picks, pick::kMemory), togglePick(picks, pick::kMemory);
    expect(picks.picked == std::vector<std::string>{ pick::kDisk, pick::kMemory }, "picked again, it's the newest");

    // Devices: known once seen with a level, the latest seen last; picked by name.
    expect(rememberDevices(picks, fullDevices()) && picks.devices == std::vector<std::string>{ "Keychron K3", "MX Master 3S", "WH-1000XM5" } &&
           !rememberDevices(picks, fullDevices()), "devices seen are remembered, once");
    expect(!rememberDevices(picks, { { "Speaker", std::nullopt }, { "", 50.0f }, { "Two\nlines", 50.0f } }), "not one with no level or no name on one line");
    expect(rememberDevices(picks, { { "Keychron K3", 40.0f } }) && picks.devices.back() == "Keychron K3", "seen again: the latest");
    expect(togglePick(picks, devicePick("WH-1000XM5")) && devicePicked(picks) && isPicked(picks, "bt:WH-1000XM5") && !isPicked(picks, pick::kDisk),
           "a known device's battery is a pick like the others");
    std::vector<BluetoothDevice> many;
    for (std::size_t i = 0; i < t.statsKnownDevices + 3; ++i) many.push_back({ "Device " + std::string(1, static_cast<char>('a' + i)), 50.0f });
    rememberDevices(picks, many);
    expect(picks.devices.size() == t.statsKnownDevices && picks.devices.back() == many.back().name && picks.devices.front() == "WH-1000XM5",
           "more than are kept: the ones seen longest ago go, never a picked one");

    // Kept as text, and read back through the same rules.
    StatsPicks back = loadPicks(savePicks(picks));
    expect(back == picks, "saved and loaded: the same picks, the same devices, in order");
    StatsPicks odd = loadPicks("pick cpu\r\npick nonsense\ndevice Buds\npick bt:Buds\npick bt:Gone\npick memory\npick cpu\n\ngarbage\ndevice Buds\n");
    expect(odd.devices == std::vector<std::string>{ "Buds" } && odd.picked == std::vector<std::string>{ "bt:Buds", pick::kMemory },
           "a damaged file: what's real, no more than fit, the rest skipped");
    expect(loadPicks("") == StatsPicks{} && savePicks({}).empty(), "nothing saved: nothing picked");

    // Settings' switches: every reading, then the devices, the latest seen first.
    auto choices = choicesOf(odd);
    expect(choices.size() == 7 && choices[0] == StatsChoice{ pick::kCpuLoad, "CPU load", false, false } && choices[1].id == pick::kCpuTemperature &&
           choices[3] == StatsChoice{ pick::kMemory, "Memory", true, false } && choices[6] == StatsChoice{ "bt:Buds", "Buds", true, true },
           "a switch per reading and per device, on where picked");
    expect(choicesOf({}).size() == 6, "no devices seen: the six readings");
}

void pillTests()
{
    namespace pick = stats_pick;
    StatsReading r = fullReading();
    auto line = [&](std::vector<std::string> picked, const StatsReading& reading) {
        StatsPicks picks{ .picked = std::move(picked), .devices = { "WH-1000XM5", "Galaxy Buds2 Pro Max" } };
        return pillText(reading, reading.bluetooth.value_or(std::vector<BluetoothDevice>{}), picks);
    };
    expect(line({}, r).empty(), "nothing picked: no pill");
    expect(line({ pick::kCpuLoad }, r) == "CPU 14%" && line({ pick::kCpuTemperature }, r) == "CPU 46°", "one reading");
    expect(line({ pick::kCpuTemperature, pick::kCpuLoad }, r) == "CPU 14% · 46°", "the CPU's two under one name, as the mockup's");
    expect(line({ pick::kNetwork, pick::kGpuLoad }, r) == "GPU 13% · ↓ 9.9 ↑ 0.3 MB/s" && line({ pick::kDisk, pick::kMemory }, r) == "RAM 52% · Disk 71%",
           "in the card's order, whatever the order picked");
    expect(line({ "bt:WH-1000XM5", pick::kMemory }, r) == "RAM 52% · WH-1000XM5 82%", "a device: its name and its level");
    // A pick with no reading is left out, not shown as a dash.
    StatsReading sparse{ .cpuLoad = 40, .memoryUsed = 8 * kGb, .memoryTotal = 16 * kGb };
    expect(line({ pick::kCpuTemperature, pick::kCpuLoad }, sparse) == "CPU 40%" && line({ pick::kGpuLoad, pick::kMemory }, sparse) == "RAM 50%",
           "no temperature, no GPU on this PC: skipped");
    expect(line({ pick::kCpuTemperature }, sparse).empty() && line({ "bt:WH-1000XM5", pick::kDisk }, sparse).empty() && line({ pick::kNetwork }, {}).empty(),
           "nothing picked has a reading: no pill");
    StatsReading buds = sparse;
    buds.bluetooth = std::vector<BluetoothDevice>{ { "Galaxy Buds2 Pro Max", 74.0f, DeviceKind::Earbuds } };
    expect(line({ "bt:Galaxy Buds2 Pro Max" }, buds) == "Galaxy Buds… 74%", "a long name cut to fit");

    // What the reader is asked for.
    StatsRequest all = cardRequest(true);
    expect(all.cpuLoad && all.cpuTemperature && all.gpuLoad && all.gpuTemperature && all.memory && all.disk && all.network && all.bluetooth &&
           !cardRequest(false).bluetooth && cardRequest(false).any(), "the card: everything; Bluetooth when it's due");
    StatsPicks two{ .picked = { pick::kCpuTemperature, pick::kNetwork }, .devices = { "Buds" } };
    expect(pillRequest(two, true) == StatsRequest{ .cpuTemperature = true, .network = true } && pillRequest(two, false) == pillRequest(two, true),
           "the pill: only what's picked; no device picked, Bluetooth is never asked");
    StatsPicks device{ .picked = { "bt:Buds", pick::kGpuLoad }, .devices = { "Buds" } };
    expect(pillRequest(device, true) == StatsRequest{ .gpuLoad = true, .bluetooth = true } && pillRequest(device, false) == StatsRequest{ .gpuLoad = true },
           "a device picked: Bluetooth when it's due");
    expect(!pillRequest({}, true).any() && !StatsRequest{}.any() && StatsRequest{ .disk = true }.any(), "nothing picked: nothing asked");
}

} // namespace

void runStatisticsTests()
{
    cleaningTests();
    levelTests();
    deviceTests();
    pickTests();
    pillTests();
    runStatisticsCardTests();
    runStatisticsProviderTests();
}

} // namespace cup::test
