// The statistics provider (decisions 171, 174): what it asks the reader for
// and when, its pill as an activity under every other, its picks kept, and
// its switches in Settings; through the registry and the island like the app's loop.

#include "core/layout.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/media/now_playing.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"
#include "providers/statistics/statistics_provider.h"
#include "providers/system/settings_provider.h"
#include "statistics_fixture.h"

#include <memory>

namespace cup::test {
namespace {

namespace pick = stats_pick;

// The registry and the island, driven like the app's loop. `saved`: what the store holds for its picks.
struct Island {
    IslandState state;
    Ms now{ 0 };
    Ms wall{ 0 }; // wall time at now == 0
    std::string* saved;
    ProviderRegistry registry;
    StatisticsProvider* stats = nullptr;
    std::vector<StatsRequest> asked;
    std::optional<ShownCard> card;
    int listChanges = 0;

    explicit Island(std::string* savedPicks)
        : saved(savedPicks),
          registry([this](const Event& e, const char*) { state = reduce(state, e, now); }, [this] { return now; },
                   { .load = [this](const std::string&, const std::string& key) { return key == StatisticsProvider::kPicksKey ? *saved : std::string(); },
                     .save = [this](const std::string&, const std::string& key, const std::string& text) {
                         if (key == StatisticsProvider::kPicksKey) *saved = text;
                     },
                     .wallClock = [this] { return wall + now; } })
    {
        auto provider = std::make_unique<StatisticsProvider>([this](const StatsRequest& request) { asked.push_back(request); });
        stats = provider.get();
        stats->onChoicesChanged([this] { ++listChanges; });
        registry.add(std::move(provider));
        registry.start("statistics");
    }
    // An event, then what the app does after each: tells providers whose card is shown now.
    void send(const Event& e)
    {
        state = reduce(state, e, now);
        auto next = shownCard(state);
        if (next == card) return;
        if (card) registry.cardClosed(*card);
        if (next) registry.cardShown(*next);
        card = next;
    }
    void wake()
    {
        if (auto at = registry.nextWake()) now = *at;
        registry.wakeDue(now);
    }
    // The card's content: its activity's while the pill shows something, else its panel's.
    const Content& content() const
    {
        if (const Activity* a = findActivity(state, StatisticsProvider::kPanelId)) return a->content;
        return findPanel(state, StatisticsProvider::kPanelId)->content;
    }
    bool live() const { return findActivity(state, StatisticsProvider::kPanelId) != nullptr; }
    std::string pill() const
    {
        const Content& c = content();
        return hasPill(c) ? std::get<Text>(c.compactCenter[0].items[0]).value : "";
    }
};

// With nothing picked: readings only while the card is shown, and none kept after.
void cardScheduleTests()
{
    const Tokens& t = tokens();
    std::string saved;
    Island island(&saved);
    expect(island.state.panels.size() == 1 && island.state.panels[0].icon == Glyph::Gauge && isPage(island.state.panels[0]) &&
           island.state.panels[0].providerId == "statistics", "a page with a gauge in the page row");
    expect(island.content() == statisticsContent({}) && island.asked.empty() && !island.registry.nextWake() && !island.live(),
           "closed, nothing picked: an empty card, nothing asked, nothing scheduled, no pill");
    island.stats->publish(fullReading());
    expect(island.content() == statisticsContent({}), "a reading nobody is waiting for is dropped");

    island.send(PillClicked{});
    expect(mode(island.state) == Mode::Expanded && island.asked == std::vector<StatsRequest>{ cardRequest(true) } &&
           island.registry.nextWake() == island.now + t.statsRefresh, "shown: everything asked at once, Bluetooth too; the next statsRefresh on");
    island.stats->publish(fullReading());
    expect(island.content() == statisticsContent(viewOf(fullReading())) && !island.live() && island.state.activities.empty(),
           "the answer shows; nothing picked, so no activity and no pill");
    // Readings every statsRefresh; Bluetooth only every statsBluetoothRefresh.
    std::size_t listed = 1;
    Ms listedAt = island.now;
    for (int i = 0; i < 12; ++i) {
        island.wake();
        bool due = island.now - listedAt >= t.statsBluetoothRefresh;
        expect(island.asked.back() == cardRequest(due), "every statsRefresh: everything, the devices only when due");
        if (due) listedAt = island.now, ++listed;
    }
    expect(island.asked.size() == 13 && listed == 1 + static_cast<std::size_t>(12 * t.statsRefresh.count() / t.statsBluetoothRefresh.count()) && listed < 6,
           "Bluetooth is listed a fraction as often");
    // A reading without a listing keeps the last one.
    StatsReading next = fullReading();
    next.cpuLoad = 97, next.bluetooth.reset();
    island.stats->publish(next);
    expect(std::get<StatTile>(island.content().expandedCenter[0].items[0]).value == "97%" && std::get<StatTile>(island.content().expandedCenter[0].items[0]).alert &&
           std::get<BatteryTile>(island.content().expandedTrailing[0].items[0]) == bluetoothTile(fullDevices()), "a new reading replaces the last; the devices stay");
    next.bluetooth = std::vector<BluetoothDevice>{};
    island.stats->publish(next);
    expect(std::get<BatteryTile>(island.content().expandedTrailing[0].items[0]).rows.empty(), "listed, and none connected: gone from the tile");

    std::size_t before = island.asked.size();
    island.send(EscapePressed{});
    expect(mode(island.state) != Mode::Expanded && !island.registry.nextWake() && island.content() == statisticsContent({}),
           "closed: no more wake-ups, and no old numbers left for next time");
    island.stats->publish(fullReading());
    expect(island.content() == statisticsContent({}) && island.asked.size() == before, "a slow read's answer after closing is dropped");
    island.send(PillClicked{});
    expect(island.asked.size() == before + 1 && island.asked.back() == cardRequest(true), "opened again: everything again, the devices at once");
    expect(saved == "device Keychron K3\ndevice MX Master 3S\ndevice WH-1000XM5\n" && island.listChanges == 1,
           "the devices it saw are kept for Settings' list, and Settings is told once");
}

// With readings picked: only those while the card is closed, and the pill shows them.
void pillScheduleTests()
{
    const Tokens& t = tokens();
    std::string saved;
    Island island(&saved);
    island.stats->toggleChoice(pick::kCpuLoad);
    expect(saved == "pick cpu\n" && island.asked == std::vector<StatsRequest>{ { .cpuLoad = true } } &&
           island.registry.nextWake() == island.now + t.statsPillRefresh && !island.live(),
           "the first pick with the card closed: saved, asked for at once, and again every statsPillRefresh; no pill before a reading");
    island.stats->publish({ .cpuLoad = 14 });
    expect(island.live() && island.pill() == "CPU 14%" && mode(island.state) == Mode::Compact && island.state.activities[0].level == Level::Passive &&
           island.state.panels.size() == 1, "a reading: the pill shows it, as a passive activity of its page");
    island.stats->toggleChoice(pick::kCpuTemperature);
    expect(island.asked.size() == 1 && island.pill() == "CPU 14%", "one more pick: read with the next, nothing asked twice");
    island.wake();
    expect(island.asked.back() == StatsRequest{ .cpuLoad = true, .cpuTemperature = true } && island.registry.nextWake() == island.now + t.statsPillRefresh,
           "only what's picked is asked for");
    island.stats->publish({ .cpuLoad = 15, .cpuTemperature = 46 });
    expect(island.pill() == "CPU 15% · 46°", "both in the pill");
    island.stats->publish({ .cpuLoad = 16 });
    expect(island.pill() == "CPU 16%" && island.live(), "a picked reading that isn't there is left out");
    island.stats->publish({});
    expect(!island.live() && mode(island.state) == Mode::Idle && island.registry.nextWake().has_value(), "none of them there: no pill, but still asked for");
    island.stats->publish({ .cpuLoad = 17 });

    // The card over a live pill: everything while it's shown, then the pill's again, with what it had.
    island.send(PillClicked{});
    expect(mode(island.state) == Mode::Expanded && island.asked.back() == cardRequest(true) && island.registry.nextWake() == island.now + t.statsRefresh,
           "the pill clicked: its card, and everything asked for");
    island.stats->publish(fullReading());
    expect(std::get<StatTile>(island.content().expandedCenter[0].items[1]).caption == "GPU" && island.pill() == "CPU 14% · 46°", "the card, full; the pill follows");
    island.send(EscapePressed{});
    expect(island.live() && island.pill() == "CPU 14% · 46°" && island.registry.nextWake() == island.now + t.statsPillRefresh,
           "closed with readings picked: the pill stays, the next reading statsPillRefresh on");
    island.wake();
    expect(island.asked.back() == StatsRequest{ .cpuLoad = true, .cpuTemperature = true }, "...and only the pill's are asked for again");

    // A third pick turns off the one picked longest ago; the last one off, everything stops.
    island.stats->toggleChoice(pick::kMemory);
    auto on = [&] {
        std::vector<std::string> ids;
        for (const StatsChoice& c : island.stats->choices())
            if (c.on) ids.push_back(c.id);
        return ids;
    };
    expect(on() == std::vector<std::string>{ pick::kCpuTemperature, pick::kMemory } && saved.find("pick cputemp\npick memory\n") != std::string::npos,
           "a third pick: the oldest gives way");
    island.stats->toggleChoice("nonsense");
    expect(on().size() == 2, "an unknown id changes nothing");
    island.stats->toggleChoice(pick::kMemory), island.stats->toggleChoice(pick::kCpuTemperature);
    std::size_t before = island.asked.size();
    expect(on().empty() && !island.live() && !island.registry.nextWake() && island.content() == statisticsContent(viewOf({}, loadPicks(saved))) &&
           saved.find("pick") == std::string::npos, "the last pick off, the card closed: no pill, nothing asked, nothing kept");
    island.stats->publish(fullReading());
    expect(!island.live() && island.asked.size() == before, "a late answer is dropped");

    // Picks survive a restart: the readings start with the app.
    saved = "device Buds\npick disk\npick bt:Buds\n";
    Island again(&saved);
    expect(again.asked == std::vector<StatsRequest>{ { .disk = true, .bluetooth = true } } && again.registry.nextWake() == again.now + t.statsPillRefresh &&
           again.stats->choices().size() == 7 && again.stats->choices()[6] == StatsChoice{ "bt:Buds", "Buds", true, true },
           "started with picks saved: asked for at once, the device with them");
    StatsReading reading{ .diskUsed = 71, .diskTotal = 100, .bluetooth = std::vector<BluetoothDevice>{ { "Buds", 74.0f, DeviceKind::Earbuds } } };
    again.stats->publish(reading);
    expect(again.pill() == "Disk 71% · Buds 74%" && again.listChanges == 0, "the pill, from the first reading; a device already known isn't news");
    // The disk every statsPillRefresh; the device's battery once a minute.
    int listings = 0;
    Ms start = again.now;
    while (again.now - start < 3 * t.statsPillBluetoothRefresh) {
        again.wake();
        expect(again.asked.back().disk && !again.asked.back().cpuLoad && !again.asked.back().network, "the disk, and nothing unpicked");
        if (again.asked.back().bluetooth) ++listings;
    }
    expect(listings == 3 && again.asked.size() == 1 + static_cast<std::size_t>(3 * t.statsPillBluetoothRefresh.count() / t.statsPillRefresh.count()),
           "Bluetooth once a minute while the disk is read every few seconds");
    again.stats->publish({ .diskUsed = 71, .diskTotal = 100 });
    expect(again.pill() == "Disk 71% · Buds 74%", "between listings the device's last level stands");
    again.stats->publish({ .diskUsed = 71, .diskTotal = 100, .bluetooth = std::vector<BluetoothDevice>{} });
    expect(again.pill() == "Disk 71%", "disconnected: out of the pill, not a dash");
}

// Only a Bluetooth device picked: one wake-up a minute, on the minute, like the idle clock's.
void minuteTests()
{
    const Tokens& t = tokens();
    Ms minute = t.statsPillBluetoothRefresh;
    std::string saved = "device Buds\npick bt:Buds\n";
    Island island(&saved);
    expect(island.asked == std::vector<StatsRequest>{ { .bluetooth = true } }, "started with a device picked: listed at once");
    // From here the wall clock is 20 s past a minute when the island's own is on one.
    island.wall = Ms{ 7 * minute.count() + 20'000 };
    island.wake();
    Ms first = island.now;
    expect((island.wall + *island.registry.nextWake()).count() % minute.count() == 0, "the next wake-up is on the wall clock's minute");
    std::size_t before = island.asked.size();
    for (int i = 0; i < 3; ++i) {
        island.wake();
        expect((island.wall + island.now).count() % minute.count() == 0 && island.asked.back() == StatsRequest{ .bluetooth = true },
               "woken on the minute, for the devices only");
    }
    expect(island.asked.size() == before + 3 && island.now - first <= 3 * minute, "once a minute, nothing between");
    island.stats->publish({ .bluetooth = std::vector<BluetoothDevice>{ { "Buds", 12.0f, DeviceKind::Earbuds } } });
    expect(island.pill() == "Buds 12%" && island.live(), "its level in the pill");
}

// Under media and the timer, whatever started last.
void rankingTests()
{
    std::string saved = "pick cpu\n";
    Island island(&saved);
    NowPlaying song{ .title = "Song", .artist = "Artist", .timeline = { .duration = Ms{ 200000 }, .playing = true } };
    island.send(ActivityStarted{ { .id = "media", .relevance = 0.6f, .content = nowPlayingContent(song), .providerId = "media" } });
    island.stats->publish({ .cpuLoad = 14 }); // started after the song
    expect(island.state.activities.size() == 2 && island.state.activities[0].id == "media" && island.state.activities[1].id == StatisticsProvider::kPanelId &&
           layout(island.state, island.now).activityId == "media", "a song keeps the pill; statistics is second, in the bubble");
    island.send(ActivityStarted{ { .id = "timer", .relevance = 0.4f, .content = { .label = "Focus" }, .providerId = "pomodoro" } });
    island.stats->publish({ .cpuLoad = 15 }); // updated after the timer started
    Scene scene = layout(island.state, island.now);
    expect(island.state.activities[0].id == "media" && island.state.activities[1].id == "timer" && island.state.activities[2].id == StatisticsProvider::kPanelId &&
           scene.activityId == "media" && scene.bubble && scene.bubble->activityId == "timer", "with a timer too: statistics is third, and shows nowhere");
    island.send(ActivityEnded{ "media" });
    island.send(ActivityEnded{ "timer" });
    expect(layout(island.state, island.now).activityId == StatisticsProvider::kPanelId && mode(island.state) == Mode::Compact, "alone again: the pill is its own");

    // Its card beside another page's is the height the timer's is, pill or not.
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    island.send(CardOpened{ StatisticsProvider::kPanelId });
    island.stats->publish(fullReading());
    float statsHeight = layout(island.state, island.now).size.y;
    island.send(CardOpened{ PomodoroProvider::kPanelId });
    expect(layout(island.state, island.now).size.y == statsHeight && statsHeight == tokens().expandedSize.y + tabStripSpace(island.state),
           "switching to the timer doesn't resize the island");
    island.registry.stop("statistics");
    expect(island.state.panels.size() == 1 && island.state.activities.empty(), "stopped: its page and its pill are gone");
    std::string kept = saved;
    island.stats->publish(fullReading()); // must do nothing, and not crash
    island.stats->toggleChoice(pick::kDisk);
    expect(saved == kept && saved.find("pick disk") == std::string::npos, "...and nothing is saved by a stopped provider");
}

// Settings' Statistics page: a switch per reading and per device seen; the rule is the provider's.
void settingsTests()
{
    std::string saved = "device Buds\n";
    Island island(&saved);
    PanelHost host;
    SettingsProvider settings({ .listStatistics = [&] {
                                    std::vector<SettingsChoice> choices;
                                    for (const StatsChoice& c : island.stats->choices()) choices.push_back({ c.id, c.name, c.on, c.device });
                                    return choices;
                                },
                                .toggleStatistic = [&](const std::string& id) { island.stats->toggleChoice(id); },
                                .statisticsMost = tokens().statsPillReadings });
    island.stats->onChoicesChanged([&] { settings.statisticsChanged(); });
    settings.start(host);
    settings.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kOpenWindow, 0 });
    const WindowPage* page = pageOf(host, kStatisticsPage);
    expect(page && host.window->pages.size() == 4 && host.window->pages[3].id == kStatisticsPage && page->title == "Statistics" && page->glyph == Glyph::Gauge &&
           page->subtitle.find("2 readings at most") != std::string::npos, "a Statistics page in Settings' sidebar, saying how many fit");
    if (!page) return;
    expect(page->sections.size() == 2 && page->sections[0].title == "Readings" && page->sections[0].rows.size() == 6 &&
           page->sections[1].title == "Bluetooth batteries" && page->sections[1].rows.size() == 1 && nameOf(page->sections[1].rows[0]) == "Buds",
           "a switch per reading, and per Bluetooth device seen");
    auto switchOf = [&](const std::string& name) { return itemOf<Switch>(windowRow(host, name)); };
    expect(switchOf("CPU load") && !switchOf("CPU load")->on && switchOf("CPU load")->actionId == std::string(settings_action::kToggleStatistic) + "0" &&
           switchOf("Network speed") && switchOf("Buds")->actionId == std::string(settings_action::kToggleStatistic) + "6", "each off, with its own action");
    auto flip = [&](const std::string& name) { settings.onAction({ "system.settings", "", switchOf(name)->actionId, 0, "", kSettingsWindowId }); };
    flip("CPU load"), flip("Buds");
    expect(switchOf("CPU load")->on && switchOf("Buds")->on && saved == "device Buds\npick cpu\npick bt:Buds\n" &&
           island.asked == std::vector<StatsRequest>{ { .cpuLoad = true } }, "flipped: on in the page, kept by statistics, the first asked for at once");
    island.wake();
    expect(island.asked.back() == StatsRequest{ .cpuLoad = true, .bluetooth = true }, "...and the device with the next reading");
    flip("Memory");
    expect(!switchOf("CPU load")->on && switchOf("Buds")->on && switchOf("Memory")->on, "a third: the oldest goes off, in the page too");
    // A device seen for the first time joins the list while the page is open.
    island.stats->publish({ .bluetooth = std::vector<BluetoothDevice>{ { "MX Master 3S", 60.0f, DeviceKind::Mouse } } });
    expect(switchOf("MX Master 3S") && !switchOf("MX Master 3S")->on && pageOf(host, kStatisticsPage)->sections[1].rows.size() == 2 &&
           nameOf(pageOf(host, kStatisticsPage)->sections[1].rows[0]) == "MX Master 3S", "a new device: its switch appears, the latest first");
    settings.onAction({ "system.settings", "", std::string(settings_action::kToggleStatistic) + "99", 0, "", kSettingsWindowId });
    expect(switchOf("Buds")->on && switchOf("Memory")->on, "an action for a switch that isn't there changes nothing");
    // No statistics provider behind it: no page.
    PanelHost plain;
    SettingsProvider without(SettingsHooks{});
    without.start(plain);
    without.onAction({ "system.settings", SettingsProvider::kPanelId, settings_action::kOpenWindow, 0 });
    expect(plain.window && plain.window->pages.size() == 3 && !pageOf(plain, kStatisticsPage), "without the hooks: Settings as it was");
}

} // namespace

void runStatisticsProviderTests()
{
    cardScheduleTests();
    pillScheduleTests();
    minuteTests();
    rankingTests();
    settingsTests();
}

} // namespace cup::test
