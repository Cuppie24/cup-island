// The statistics page (decision 171): cleaning and formatting a reading,
// which Bluetooth devices show, the card laid out by the real layout(), and
// the provider's asking, through the registry like the app's loop.

#include "core/layout.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/media/now_playing.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"
#include "providers/statistics/statistics_provider.h"

#include <cmath>
#include <limits>
#include <memory>

namespace cup::test {
namespace {

constexpr std::uint64_t kGb = 1024ull * 1024 * 1024;
const float kNan = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

// A laptop with everything readable.
StatsReading fullReading()
{
    return { .cpuLoad = 42.4f, .cpuTemperature = 61.2f, .gpuLoad = 17, .gpuTemperature = 55,
             .memoryUsed = kGb * 101 / 10, .memoryTotal = 16 * kGb, .diskUsed = 71 * kGb, .diskTotal = 100 * kGb,
             .networkDown = 1.2f * 1024 * 1024, .networkUp = 340 * 1024.0f, .battery = 82, .charging = false,
             .bluetooth = { { "AirPods Pro", 80.0f }, { "MX Master 3", 45.0f } } };
}

std::string textAt(const Slot& slot, std::size_t row, std::size_t item)
{
    if (row >= slot.size() || item >= slot[row].items.size()) return "?";
    auto* text = std::get_if<Text>(&slot[row].items[item]);
    return text ? text->value : "?";
}

void cleaningTests()
{
    expect(cleanPercent(42.4f) == 42.4f && !cleanPercent(std::nullopt), "a percent passes; none stays none");
    expect(!cleanPercent(kNan) && !cleanPercent(kInf) && !cleanPercent(-kInf), "not a number: no reading");
    expect(cleanPercent(-3.0f) == 0.0f && cleanPercent(104.0f) == 100.0f, "clamped to 0..100");
    expect(percentText(42.4f) == "42%" && percentText(42.5f) == "43%" && percentText(99.6f) == "100%" && percentText(0.2f) == "0%",
           "to the nearest whole");
    expect(percentText(std::nullopt) == kNoReading && percentText(kNan) == kNoReading && percentText(250.0f) == "100%", "none: a dash");

    expect(temperatureText(61.2f) == "61 °C" && temperatureText(61.5f) == "62 °C", "a temperature, whole degrees");
    expect(temperatureText(std::nullopt).empty() && temperatureText(kNan).empty(), "none: nothing");
    expect(temperatureText(0.0f).empty() && temperatureText(-273.15f).empty(), "an unread sensor's 0 or below: nothing");
    expect(temperatureText(static_cast<float>(tokens().statsPlausibleTemperature) + 1).empty() &&
           !temperatureText(static_cast<float>(tokens().statsPlausibleTemperature)).empty(), "past plausible: nothing, never clamped");

    expect(usedPercent(8 * kGb, 16 * kGb) == 50.0f, "used of total");
    expect(!usedPercent(8 * kGb, std::nullopt) && !usedPercent(std::nullopt, 16 * kGb) && !usedPercent(1, 0), "needs both, and a total");
    expect(usedPercent(20 * kGb, 16 * kGb) == 100.0f, "more used than there is: full");
    expect(memoryText(kGb * 101 / 10, 16 * kGb) == "10.1 / 16 GB", "one decimal, none for .0");
    expect(memoryText(kGb * 159 / 10, kGb * 159 / 10) == "15.9 / 15.9 GB" && memoryText(100 * kGb, kGb * 1283 / 10) == "100 / 128 GB",
           "three digits at most");
    expect(memoryText(20 * kGb, 16 * kGb) == "16 / 16 GB" && memoryText(std::nullopt, 16 * kGb).empty() && memoryText(1, 0).empty(),
           "used capped at the total; nothing without both");

    expect(speedText(0.0f) == "0 KB/s" && speedText(500.0f) == "0 KB/s" && speedText(340 * 1024.0f) == "340 KB/s", "kilobytes, whole");
    expect(speedText(1.2f * 1024 * 1024) == "1.2 MB/s" && speedText(12.4f * 1024 * 1024) == "12 MB/s" && speedText(2.0f * 1024 * 1024) == "2 MB/s",
           "megabytes: a decimal under 10");
    expect(speedText(999.4f * 1024) == "999 KB/s" && speedText(999.6f * 1024) == "1 MB/s" && speedText(1023.0f * 1024) == "1 MB/s",
           "never four digits: the next unit");
    expect(speedText(1.1f * 1024 * 1024 * 1024) == "1.1 GB/s" && speedText(5000.0f * 1024 * 1024 * 1024) == "5000 GB/s", "gigabytes at the top");
    expect(speedText(std::nullopt).empty() && speedText(-1.0f).empty() && speedText(kNan).empty() && speedText(kInf).empty(), "bad: nothing");
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

    StatsReading r = fullReading();
    expect(!anythingHigh(r) && !anythingHigh({}), "an easy day; nothing read");
    r.gpuTemperature = hot;
    expect(anythingHigh(r), "one hot reading is enough");
    r = fullReading();
    r.memoryUsed = r.memoryTotal;
    expect(anythingHigh(r), "memory full counts");
    r = fullReading();
    r.diskUsed = r.diskTotal;
    expect(!anythingHigh(r), "a full disk doesn't");
    expect(!statisticsContent(fullReading()).accent && statisticsContent(r).accent == std::nullopt, "no accent: the theme's fill");
    r.cpuLoad = 97;
    expect(statisticsContent(r).accent == t.dangerColor, "high: the fills in the danger colour");
}

void bluetoothTests()
{
    std::vector<BluetoothDevice> devices = { { "Keyboard", 90.0f }, { "Mouse", 15.2f }, { "Speaker", std::nullopt }, { "", 50.0f },
                                             { "Buds", 15.4f }, { "Pen", kNan }, { "Headset", 140.0f } };
    ShownDevices all = shownDevices(devices, 10);
    expect(all.more == 0 && all.devices.size() == 4, "only named devices with a battery level");
    expect(all.devices[0].name == "Buds" && all.devices[1].name == "Mouse" && all.devices[2].name == "Keyboard" && all.devices[3].name == "Headset",
           "the emptiest first; by name where they read the same");
    expect(all.devices[3].battery == 100.0f, "its level cleaned");
    ShownDevices three = shownDevices(devices, 3);
    expect(three.devices.size() == 2 && three.more == 2 && three.devices[1].name == "Mouse", "more than fit: the last place counts the rest");
    ShownDevices four = shownDevices(devices, 4);
    expect(four.devices.size() == 4 && four.more == 0, "exactly as many as fit: all of them");
    expect(shownDevices(devices, 1).devices.empty() && shownDevices(devices, 1).more == 4 && shownDevices(devices, 0).more == 4, "no room: only the count");
    expect(shownDevices({}, 3) == ShownDevices{}, "none");

    std::size_t most = tokens().statsDeviceNameMaxLength;
    std::string exact(most, 'a'), longer(most + 1, 'a');
    expect(shortName("AirPods Pro") == "AirPods Pro" && shortName(exact) == exact, "a name that fits is left alone");
    expect(shortName(longer) == std::string(most - 1, 'a') + "…", "too long: cut, the \"…\" in its last place");
    std::string accented;
    for (std::size_t i = 0; i < most + 2; ++i) accented += "é"; // two bytes each
    std::string cut = shortName(accented);
    expect(cut.size() == 2 * (most - 1) + 3 && cut.substr(0, 2) == "é", "counted in characters, never cut inside one");
    expect(shortName(std::string(most - 2, 'a') + " bcdef") == std::string(most - 2, 'a') + "…", "no space before the \"…\"");

    StatsReading r;
    expect(!bluetoothRow(r), "no devices: no row");
    r.bluetooth = devices;
    Row row = *bluetoothRow(r);
    expect(row.items.size() == tokens().statsBluetoothShown && row.even && row.apart, "a column each, in a group of their own");
    const Text& first = std::get<Text>(row.items[0]);
    expect(first.value == "Buds 15%" && first.bright == std::vector<std::pair<std::size_t, std::size_t>>{ { 5, 3 } }, "the name, its level bright");
    expect(std::get<Text>(row.items.back()).value == "+2 more", "then how many more");
}

void cardTests()
{
    const Tokens& t = tokens();
    Content card = statisticsContent(fullReading());
    const Slot& rows = card.expandedCenter;
    expect(card.card == CardLayout::Centred && card.noIdleTimeout && rows.size() == 6, "four rows of tiles, the rest, Bluetooth");
    expect(card.compactCenter.empty() && card.minimal.empty(), "a page only: nothing for the pill");
    expect(textAt(rows, 0, 0) == "CPU" && textAt(rows, 0, 1) == "GPU" && textAt(rows, 0, 2) == "Memory", "the tiles' names");
    expect(textAt(rows, 1, 0) == "42%" && textAt(rows, 1, 1) == "17%" && textAt(rows, 1, 2) == "63%", "their percents");
    expect(std::get<Text>(rows[1].items[0]).style == TextStyle::CardTitle && std::get<Text>(rows[0].items[0]).style == TextStyle::Caption,
           "the value heavy, its name quiet");
    const Progress& bar = std::get<Progress>(rows[2].items[0]);
    expect(std::abs(bar.timeline.fractionAt(Ms{ 0 }) - 0.424f) < 0.001f && bar.timeline.fractionAt(Ms{ 60000 }) == bar.timeline.fractionAt(Ms{ 0 }) &&
           bar.shape == ProgressShape::Bar && !bar.seekable, "a bar at its level, standing still");
    expect(textAt(rows, 3, 0) == "61 °C" && textAt(rows, 3, 1) == "55 °C" && textAt(rows, 3, 2) == "10.1 / 16 GB", "the details under the bars");
    for (std::size_t r = 0; r < 4; ++r) expect(rows[r].even && rows[r].items.size() == 3 && rows[r].loose, "a table: columns line up");
    expect(otherTexts(fullReading()) == std::vector<std::string>{ "Disk 71%", "↓ 1.2 MB/s", "↑ 340 KB/s", "Battery 82%" }, "the rest, in order");
    expect(rows[4].apart && rows[4].even && textAt(rows, 4, 3) == "Battery 82%" && std::get<Text>(rows[4].items[3]).brightDigits, "...in a row");
    StatsReading plugged = fullReading();
    plugged.charging = true;
    expect(otherTexts(plugged).back() == "Charging 82%", "on power");

    // Nothing read: the same tiles, dashes and empty bars, no other rows.
    Content empty = statisticsContent({});
    const Slot& bare = empty.expandedCenter;
    expect(bare.size() == 4 && textAt(bare, 1, 0) == kNoReading && textAt(bare, 1, 2) == kNoReading && textAt(bare, 3, 0).empty(), "dashes for percents");
    expect(std::get<Progress>(bare[2].items[1]) == Progress{} && Progress{}.timeline.fractionAt(Ms{ 0 }) == 0, "empty tracks");
    expect(!otherRow({}) && !bluetoothRow({}), "no rest, no Bluetooth");
    // A desktop: no temperatures, no battery, no Bluetooth. The details keep their row.
    StatsReading desktop{ .cpuLoad = 5, .gpuLoad = kNan, .memoryUsed = 4 * kGb, .memoryTotal = 32 * kGb, .networkDown = 0 };
    const Slot desk = statisticsContent(desktop).expandedCenter;
    expect(desk.size() == 5 && textAt(desk, 1, 1) == kNoReading && textAt(desk, 3, 0).empty() && textAt(desk, 3, 2) == "4 / 32 GB" &&
           desk[4].items.size() == 1 && textAt(desk, 4, 0) == "↓ 0 KB/s", "what's missing leaves its place or its row");

    // Laid out: every piece inside the card, at the height every card shares (design rules).
    IslandState media;
    NowPlaying song{ .title = "Song", .artist = "Artist", .timeline = { .duration = Ms{ 200000 }, .playing = true } };
    media = reduce(media, ActivityStarted{ { .id = "media", .content = nowPlayingContent(song) } }, Ms{ 0 });
    media = reduce(media, PillClicked{}, Ms{ 0 });
    float mediaHeight = layout(media, Ms{ 0 }).size.y;
    StatsReading crowded = fullReading(); // the fullest the card gets
    crowded.bluetooth = { { "A very long device name indeed", 80.0f }, { "MX Master 3", 45.0f }, { "Keyboard", 10.0f }, { "Pen", 5.0f } };
    for (const StatsReading& reading : { StatsReading{}, desktop, fullReading(), crowded }) {
        IslandState state;
        state = reduce(state, PanelPublished{ statisticsPanel(reading) }, Ms{ 0 });
        state = reduce(state, PillClicked{}, Ms{ 0 }); // the idle pill opens the first page (decision 170)
        Scene scene = layout(state, Ms{ 0 });
        float height = scene.size.y - (scene.tabs ? tabStripSpace(state) : 0);
        expect(mode(state) == Mode::Expanded && height == mediaHeight && height == t.expandedSize.y && scene.size.x == t.expandedSize.x,
               "media's and the timer's card size (" + std::to_string(int(height)) + ")");
        std::size_t placed = 0;
        for (const Placed& p : scene.elements) {
            if (p.key.rfind("expandedCenter/", 0) != 0) continue;
            ++placed;
            expect(p.frame.y >= t.expandedPadding - 0.01f && p.frame.y + p.frame.height <= scene.size.y - t.expandedPadding + 0.01f &&
                   p.frame.x >= t.expandedPadding - 0.01f && p.frame.x + p.frame.width <= scene.size.x - t.expandedPadding + 0.01f,
                   "fits the card: " + p.key);
        }
        expect(placed >= 12, "the tiles are there");
        // Columns: a tile's name, percent, bar and detail share one centre.
        auto centre = [&](const std::string& key) {
            for (const Placed& p : scene.elements)
                if (p.key == key) return p.frame.x + p.frame.width / 2;
            return -1.0f;
        };
        for (int column = 0; column < 3; ++column)
            for (int r = 1; r < 4; ++r)
                expect(std::abs(centre("expandedCenter/0/" + std::to_string(column)) -
                                centre("expandedCenter/" + std::to_string(r) + "/" + std::to_string(column))) < 0.5f, "a tile's pieces in one column");
    }
}

// The registry and the island, driven like the app's loop.
struct Island {
    IslandState state;
    Ms now{ 0 };
    ProviderRegistry registry;
    StatisticsProvider* stats = nullptr;
    int asked = 0;
    std::optional<ShownCard> shown;

    Island() : registry([this](const Event& e, const char*) { state = reduce(state, e, now); }, [this] { return now; })
    {
        auto provider = std::make_unique<StatisticsProvider>([this] { ++asked; });
        stats = provider.get();
        registry.add(std::move(provider));
        registry.start("statistics");
    }
    // An event, then what the app does after each: tells providers whose card is shown now.
    void send(const Event& e)
    {
        state = reduce(state, e, now);
        auto card = shownCard(state);
        if (card == shown) return;
        if (shown) registry.cardClosed(*shown);
        if (card) registry.cardShown(*card);
        shown = card;
    }
    void wake()
    {
        if (auto at = registry.nextWake()) now = *at;
        registry.wakeDue(now);
    }
    std::string cpu() const
    {
        for (const Panel& p : state.panels)
            if (p.id == StatisticsProvider::kPanelId) return textAt(p.content.expandedCenter, 1, 0);
        return "?";
    }
};

void providerTests()
{
    const Tokens& t = tokens();
    Island island;
    expect(island.state.panels.size() == 1 && island.state.panels[0].icon == Glyph::Gauge && isPage(island.state.panels[0]) &&
           island.state.panels[0].providerId == "statistics", "a page with a gauge in the page row");
    expect(island.cpu() == kNoReading && island.asked == 0 && !island.registry.nextWake(), "closed: nothing asked, nothing scheduled");
    island.stats->publish(fullReading());
    expect(island.cpu() == kNoReading, "a reading nobody is looking at is dropped");

    island.send(PillClicked{});
    expect(mode(island.state) == Mode::Expanded && island.asked == 1 && island.registry.nextWake() == island.now + t.statsRefresh,
           "shown: a reading asked for at once, the next statsRefresh on");
    expect(island.state.activities.empty(), "never an activity: nothing in the pill");
    island.stats->publish(fullReading());
    expect(island.cpu() == "42%", "the answer shows");
    island.wake();
    expect(island.asked == 2 && island.registry.nextWake() == island.now + t.statsRefresh, "and again each statsRefresh");
    StatsReading next = fullReading();
    next.cpuLoad = 97;
    island.stats->publish(next);
    expect(island.cpu() == "97%" && island.state.panels[0].content.accent == t.dangerColor, "a new reading replaces the last");

    island.send(EscapePressed{});
    expect(mode(island.state) != Mode::Expanded && !island.registry.nextWake() && island.cpu() == kNoReading,
           "closed: no more wake-ups, and no old numbers left for next time");
    island.stats->publish(fullReading());
    expect(island.cpu() == kNoReading && island.asked == 2, "a slow read's answer after closing is dropped");
    island.send(PillClicked{});
    expect(island.asked == 3 && island.registry.nextWake().has_value(), "opened again: asking again");

    // Beside another page: its card is the height the timer's is.
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    island.send(CardOpened{ StatisticsProvider::kPanelId });
    float statsHeight = layout(island.state, island.now).size.y;
    island.send(CardOpened{ PomodoroProvider::kPanelId });
    expect(layout(island.state, island.now).size.y == statsHeight && statsHeight == t.expandedSize.y + tabStripSpace(island.state),
           "switching to the timer doesn't resize the island");
    expect(!island.registry.nextWake() && island.asked == 3, "the timer's page shown: statistics stopped asking");

    island.registry.stop("statistics");
    expect(island.state.panels.size() == 1 && island.state.panels[0].id == PomodoroProvider::kPanelId, "stopped: its page is gone");
    island.stats->publish(fullReading()); // must do nothing, and not crash
}

} // namespace

void runStatisticsTests()
{
    cleaningTests();
    levelTests();
    bluetoothTests();
    cardTests();
    providerTests();
}

} // namespace cup::test
