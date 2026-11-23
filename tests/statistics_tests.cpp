// Statistics, the numbers (decisions 171, 172): cleaning and formatting a
// reading, which rows and devices show, and the graphs' histories.
// The overview is in statistics_card_tests.cpp, its pages in statistics_page_tests.cpp,
// the provider in statistics_provider_tests.cpp.

#include "core/tokens.h"
#include "expect.h"
#include "providers/statistics/stats_format.h"
#include "providers/statistics/stats_history.h"

#include <cmath>
#include <limits>

namespace cup::test {
void runStatisticsCardTests();
void runStatisticsPageTests();
void runStatisticsProviderTests();
namespace {

constexpr std::uint64_t kMb = 1024ull * 1024, kGb = 1024 * kMb, kTb = 1024 * kGb;
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
    expect(shareText(11.63) == "11.6%" && shareText(9.0) == "9%" && shareText(0.04) == "0%" && shareText(100) == "100%" && shareText(250) == "100%" &&
           shareText(-2) == "0%" && shareText(std::nan("")) == "0%", "a share: one decimal, cleaned");

    expect(temperatureText(61.2f) == "61°" && temperatureText(61.5f) == "62°", "a temperature, whole degrees");
    expect(temperatureText(std::nullopt).empty() && temperatureText(kNan).empty(), "none: nothing");
    expect(temperatureText(0.0f).empty() && temperatureText(-273.15f).empty(), "an unread sensor's 0 or below: nothing");
    expect(temperatureText(static_cast<float>(tokens().statsPlausibleTemperature) + 1).empty() &&
           !temperatureText(static_cast<float>(tokens().statsPlausibleTemperature)).empty(), "past plausible: nothing, never clamped");

    expect(usedPercent(8 * kGb, 16 * kGb) == 50.0f, "used of total");
    expect(!usedPercent(8 * kGb, std::nullopt) && !usedPercent(std::nullopt, 16 * kGb) && !usedPercent(1, 0), "needs both, and a total");
    expect(usedPercent(20 * kGb, 16 * kGb) == 100.0f, "more used than there is: full");
}

void sizeTests()
{
    expect(sizeText(0) == "0 KB" && sizeText(310 * kMb) == "310 MB" && sizeText(kGb * 42 / 10) == "4.2 GB" && sizeText(16 * kGb) == "16 GB" &&
           sizeText(364 * kGb) == "364 GB", "a size: whole under GB, a decimal in GB");
    expect(sizeText(999 * kMb) == "999 MB" && sizeText(1000 * kMb) == "1 GB", "never four digits: the next unit");
    expect(sizeText(kTb * 131 / 100) == "1.31 TB" && sizeText(2 * kTb) == "2 TB" && sizeText(5000 * kTb) == "5000 TB", "terabytes: two decimals, the top unit");
    expect(usedOfText(kGb * 101 / 10, 16 * kGb) == "10.1 / 16 GB" && usedOfText(364 * kGb, 512 * kGb) == "364 / 512 GB", "used of total, one unit");
    expect(usedOfText(612 * kGb, 2 * kTb) == "0.6 / 2 TB" && usedOfText(100 * kGb, kGb * 1283 / 10) == "100 / 128 GB", "...the total's");
    expect(usedOfText(20 * kGb, 16 * kGb) == "16 / 16 GB" && usedOfText(std::nullopt, 16 * kGb).empty() && usedOfText(1, 0).empty(),
           "used capped at the total; nothing without both");
    expect(freeText(364 * kGb, 512 * kGb) == "148 GB free" && freeText(20 * kGb, 16 * kGb) == "0 KB free" && freeText(std::nullopt, kGb).empty(), "what's free");

    expect(speedText(0.0f) == "0 KB/s" && speedText(500.0f) == "0 KB/s" && speedText(340 * 1024.0f) == "340 KB/s", "kilobytes, whole");
    expect(speedText(1.2f * 1024 * 1024) == "1.2 MB/s" && speedText(12.4f * 1024 * 1024) == "12 MB/s" && speedText(2.0f * 1024 * 1024) == "2 MB/s",
           "megabytes: a decimal under 10");
    expect(speedText(999.4f * 1024) == "999 KB/s" && speedText(999.6f * 1024) == "1 MB/s" && speedText(1023.0f * 1024) == "1 MB/s",
           "never four digits: the next unit");
    expect(speedText(1.1f * 1024 * 1024 * 1024) == "1.1 GB/s" && speedText(5000.0f * 1024 * 1024 * 1024) == "5000 GB/s", "gigabytes at the top");
    expect(speedText(std::nullopt).empty() && speedText(-1.0f).empty() && speedText(kNan).empty() && speedText(kInf).empty(), "bad: nothing");
    expect(speedPairText(1.7f * kMb, 0.3f * kMb) == "↓ 1.7 ↑ 0.3 MB/s" && speedPairText(340.0f * 1024, 12.0f * 1024) == "↓ 340 ↑ 12 KB/s",
           "both directions, one unit");
    expect(speedPairText(12.4f * kMb, 100.0f * 1024) == "↓ 12 ↑ 0.1 MB/s", "...the faster one's");
    expect(speedPairText(std::nullopt, 2.0f * kMb) == std::string("↓ ") + kNoReading + " ↑ 2 MB/s" && speedPairText(kNan, std::nullopt).empty(),
           "one missing: a dash; both: nothing");
    expect(linkText(100'000'000) == "100 Mbps" && linkText(1'000'000'000) == "1 Gbps" && linkText(2'500'000'000ull) == "2.5 Gbps" &&
           linkText(5'400'000) == "5.4 Mbps" && linkText(0).empty() && linkText(std::nullopt).empty(), "a link's speed, decimal");
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
    // 100 Mbps is 12.5 MB/s.
    expect(atLinkLimit(12.4f * 1000 * 1000, 100'000'000) && !atLinkLimit(5.0f * 1000 * 1000, 100'000'000) && !atLinkLimit(1e9f, std::nullopt) &&
           !atLinkLimit(1e9f, 0) && !atLinkLimit(std::nullopt, 100'000'000), "a direction at its link's speed");
}

void listTests()
{
    std::vector<NamedValue> raw = { { "Spotify", 2.2 }, { "Chrome", 9.8 }, { "", 50 }, { "Broken", std::nan("") }, { "Negative", -1 },
                                    { "Code", 6.1 }, { "Beta", 2.2 }, { "Idle", 0 } };
    auto top = topValues(raw, 4);
    expect(top.size() == 4 && top[0].name == "Chrome" && top[1].name == "Code" && top[2].name == "Beta" && top[3].name == "Spotify",
           "the biggest first, by name among equals, cut to fit; no nameless or broken ones");
    expect(topValues(raw, 99).size() == 5 && topValues(raw, 0).empty() && topValues({}, 3).empty(), "as many as there are");

    std::vector<BluetoothDevice> devices = { { "Keyboard", 90.0f, DeviceKind::Keyboard }, { "Mouse", 15.2f, DeviceKind::Mouse }, { "Speaker", std::nullopt },
                                             { "", 50.0f }, { "Buds", 15.4f }, { "Pen", kNan, DeviceKind::Pen }, { "Headset", 140.0f } };
    ShownDevices all = shownDevices(devices, 10);
    expect(all.more == 0 && all.devices.size() == 4, "only named devices with a battery level");
    expect(all.devices[0].name == "Buds" && all.devices[1].name == "Mouse" && all.devices[2].name == "Keyboard" && all.devices[3].name == "Headset",
           "the emptiest first; by name where they read the same");
    expect(all.devices[3].battery == 100.0f && all.devices[1].kind == DeviceKind::Mouse, "its level cleaned, its kind kept");
    ShownDevices three = shownDevices(devices, 3);
    expect(three.devices.size() == 3 && three.more == 1 && three.devices[2].name == "Keyboard", "more than fit: the emptiest, the rest counted");
    expect(shownDevices(devices, 0).devices.empty() && shownDevices(devices, 0).more == 4 && shownDevices({}, 3) == ShownDevices{}, "no room; none");

    // Earbuds in parts: each cleaned; with no level of their own, the emptiest part's.
    BluetoothDevice buds{ "Galaxy Buds", std::nullopt, DeviceKind::Earbuds, { { "Left", 82 }, { "Right", 78 }, { "Case", 40 }, { "", 5 }, { "Bad", std::nan("") } } };
    ShownDevices parts = shownDevices({ buds, { "Speaker", std::nullopt, DeviceKind::Other, { { "Bad", std::nan("") } } } }, 5);
    expect(parts.devices.size() == 1 && parts.devices[0].parts.size() == 3 && parts.devices[0].battery == 40.0f && parts.devices[0].parts[2].name == "Case",
           "parts cleaned; the device as low as its emptiest part");
    buds.battery = 74;
    expect(shownDevices({ buds }, 5).devices[0].battery == 74.0f, "its own level wins");
    expect(kindName(DeviceKind::Earbuds) == "Earbuds" && kindName(DeviceKind::Other) == "Bluetooth device", "a kind's name");
}

void historyTests()
{
    const Tokens& t = tokens();
    StatsHistory history;
    expect(samplesOf(history, Series::CpuLoad).empty() && !anyReading(samplesOf(history, Series::CpuLoad)), "starts empty");
    StatsReading first{ .cpuLoad = 50, .cpuTemperature = 65, .memoryUsed = 4 * kGb, .memoryTotal = 16 * kGb, .networkDown = 1024.0f * 1024,
                        .networkUp = kNan, .diskRead = -5.0f };
    record(history, first);
    expect(samplesOf(history, Series::CpuLoad) == StatsHistory::Samples{ 50.0f } && samplesOf(history, Series::Memory) == StatsHistory::Samples{ 25.0f } &&
           samplesOf(history, Series::CpuTemperature) == StatsHistory::Samples{ 65.0f }, "a sample per series, cleaned");
    for (Series gap : { Series::GpuLoad, Series::GpuTemperature, Series::Upload, Series::DiskRead, Series::DiskWrite })
        expect(samplesOf(history, gap) == StatsHistory::Samples{ std::nullopt } && !anyReading(samplesOf(history, gap)),
               "missing or garbage: a gap, never a made-up value");
    // A reading that fails once leaves a hole, and the series goes on after it.
    record(history, {});
    record(history, { .cpuLoad = 150, .cpuTemperature = 400 });
    expect(samplesOf(history, Series::CpuLoad) == StatsHistory::Samples{ 50.0f, std::nullopt, 100.0f } &&
           samplesOf(history, Series::CpuTemperature) == StatsHistory::Samples{ 65.0f, std::nullopt, std::nullopt }, "a gap keeps its place in time");
    expect(percentSamples(samplesOf(history, Series::CpuLoad)) == std::vector<float>{ 0.5f, -1, 1 }, "as a graph's samples: 0..1, a gap below 0");

    for (std::size_t i = 0; i < t.statsHistoryLength + 10; ++i) record(history, { .cpuLoad = static_cast<float>(i) });
    const auto& load = samplesOf(history, Series::CpuLoad);
    expect(load.size() == t.statsHistoryLength && load.back() == static_cast<float>(t.statsHistoryLength + 9) && load.front() == 10.0f,
           "capped at statsHistoryLength: the oldest go");
    for (std::size_t i = 0; i < kSeriesCount; ++i) expect(history.series[i].size() == t.statsHistoryLength, "every series the same length");

    // Temperatures between the graph's bottom and top; the limit line where hot starts.
    float low = static_cast<float>(t.statsGraphMinTemperature), top = static_cast<float>(t.statsGraphMaxTemperature);
    auto temps = temperatureSamples({ low, top, (low + top) / 2, low - 20, top + 20, std::nullopt });
    expect(temps == std::vector<float>{ 0, 1, 0.5f, 0, 1, -1 }, "a temperature on the graph's scale, clamped");
    expect(std::abs(temperatureLimit() - (static_cast<float>(t.statsHighTemperature) - low) / (top - low)) < 0.0001f && temperatureLimit() > 0 &&
           temperatureLimit() < 1, "the limit line at statsHighTemperature");
    expect(temperatureSamples({ static_cast<float>(t.statsHighTemperature) }).front() == temperatureLimit(), "a reading at the threshold sits on the line");

    // Speeds against the series' own peak, never under the floor.
    float floor = static_cast<float>(t.statsSpeedFloor) * 1024;
    auto idle = speedSamples({ 0.0f, floor / 4, std::nullopt });
    expect(idle == std::vector<float>{ 0, 0.25f, -1 }, "an idle line lies low: the floor is the top");
    auto busy = speedSamples({ floor * 10, floor * 5, 0.0f });
    expect(busy == std::vector<float>{ 1, 0.5f, 0 }, "a busy one fills its graph: its peak is the top");
    expect(speedSamples({}).empty() && percentSamples({}).empty(), "nothing in, nothing out");
}

} // namespace

void runStatisticsTests()
{
    cleaningTests();
    sizeTests();
    levelTests();
    listTests();
    historyTests();
    runStatisticsCardTests();
    runStatisticsPageTests();
    runStatisticsProviderTests();
}

} // namespace cup::test
