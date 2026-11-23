// The statistics provider (decisions 171, 172): its pages, histories and
// asking, through the registry and the island like the app's loop.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"
#include "providers/statistics/statistics_provider.h"
#include "statistics_fixture.h"

#include <memory>

namespace cup::test {
namespace {

// The registry and the island, driven like the app's loop.
struct Island {
    IslandState state;
    Ms now{ 0 };
    ProviderRegistry registry;
    StatisticsProvider* stats = nullptr;
    int asked = 0;
    std::optional<ShownCard> card;

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
    const Content& content() const { return findPanel(state, StatisticsProvider::kPanelId)->content; }
    Scene scene() const { return layout(state, now); }
    // Click whatever sends `actionId`, where it is on the card.
    bool click(const std::string& actionId)
    {
        Scene s = scene();
        for (const Placed& p : s.elements) {
            auto hit = hitTest(s, { p.frame.x + p.frame.width / 2, p.frame.y + p.frame.height / 2 });
            if (!hit || hit->action.actionId != actionId) continue;
            registry.route(hit->action);
            return true;
        }
        return false;
    }
    // Esc, as the app handles it: the card's step back first (decision 033).
    void escape()
    {
        if (std::string back = backAction(state); !back.empty()) registry.route({ card->providerId, card->activityId, back, 0 });
        else send(EscapePressed{});
    }
};

void providerTests()
{
    const Tokens& t = tokens();
    Island island;
    expect(island.state.panels.size() == 1 && island.state.panels[0].icon == Glyph::Gauge && isPage(island.state.panels[0]) &&
           island.state.panels[0].providerId == "statistics", "a page with a gauge in the page row");
    expect(island.content() == overviewContent({}) && island.asked == 0 && !island.registry.nextWake(), "closed: the empty overview, nothing asked or scheduled");
    island.stats->publish(fullReading());
    expect(island.content() == overviewContent({}), "a reading nobody is looking at is dropped");

    island.send(PillClicked{});
    expect(mode(island.state) == Mode::Expanded && island.asked == 1 && island.registry.nextWake() == island.now + t.statsRefresh,
           "shown: a reading asked for at once, the next statsRefresh on");
    expect(island.state.activities.empty(), "never an activity: nothing in the pill");
    island.stats->publish(fullReading());
    expect(island.content() == overviewContent(viewOf(fullReading(), StatsPage::Overview, 1)), "the answer shows, its first sample in every graph");
    island.wake();
    expect(island.asked == 2 && island.registry.nextWake() == island.now + t.statsRefresh, "and again each statsRefresh");
    island.stats->publish(fullReading());
    expect(std::get<Graph>(island.content().expandedCenter[0].items[0]).samples.size() == 2, "each reading adds to the histories");

    // Tiles open pages; the arrow and Esc go back.
    for (StatsPage page : { StatsPage::Cpu, StatsPage::Gpu, StatsPage::Memory, StatsPage::Network, StatsPage::Disk, StatsPage::Bluetooth }) {
        expect(island.click(openAction(page)) && island.stats->page() == page && island.content() == statisticsContent(viewOf(fullReading(), page, 2)),
               std::string("a click on a tile opens its page: ") + openAction(page));
        expect(mode(island.state) == Mode::Expanded && island.scene().size.y == t.expandedSize.y, "the card stays open, at its height");
        expect(backAction(island.state) == stats_action::kBack, "Esc is a step back there");
        bool arrow = page == StatsPage::Cpu || page == StatsPage::Network;
        if (arrow) expect(island.click(stats_action::kBack), "the arrow");
        else island.escape();
        expect(island.stats->page() == StatsPage::Overview && island.content().backAction.empty() && mode(island.state) == Mode::Expanded,
               "back on the overview, the card still open");
    }
    island.click(stats_action::kOpenCpu);
    expect(island.scene().size.x == t.wideCardWidth, "a wide page widens the island");
    StatsReading later = fullReading();
    later.cpuLoad = 97;
    island.stats->publish(later);
    expect(island.stats->page() == StatsPage::Cpu && std::get<Text>(island.content().expandedLeading[1].items[0]).value == "97%" &&
           std::get<Graph>(island.content().expandedLeading[2].items[0]).samples.size() == 3, "a reading on a page: the page, updated");
    island.registry.route({ "statistics", StatisticsProvider::kPanelId, "nonsense", 0 });
    expect(island.stats->page() == StatsPage::Cpu, "an unknown action changes nothing");

    // A page whose block has nothing can't be opened, and closes when its readings stop.
    StatsReading noGpu = fullReading();
    noGpu.gpuLoad.reset(), noGpu.drives.clear();
    island.escape();
    island.stats->publish(noGpu);
    island.registry.route({ "statistics", StatisticsProvider::kPanelId, stats_action::kOpenGpu, 0 });
    island.registry.route({ "statistics", StatisticsProvider::kPanelId, stats_action::kOpenDisk, 0 });
    expect(island.stats->page() == StatsPage::Overview && !island.click(stats_action::kOpenGpu) && !island.click(stats_action::kOpenDisk) &&
           !hasPage(noGpu, StatsPage::Gpu) && !hasPage(noGpu, StatsPage::Disk) && hasPage(noGpu, StatsPage::Bluetooth) && hasPage({}, StatsPage::Cpu),
           "no GPU, no drives: no tiles, no pages");
    island.stats->publish(fullReading());
    island.click(stats_action::kOpenGpu);
    island.stats->publish(noGpu);
    expect(island.stats->page() == StatsPage::Overview && island.content().backAction.empty(), "the GPU's readings stop while its page is open: the overview");
    expect(statisticsContent(viewOf(noGpu, StatsPage::Gpu)) == overviewContent(viewOf(noGpu, StatsPage::Gpu)), "...whoever builds the card");

    // Closing forgets: no old numbers, no graph with a hole, the overview next time.
    island.stats->publish(fullReading());
    island.click(stats_action::kOpenMemory);
    island.escape(); // back
    island.click(stats_action::kOpenMemory);
    island.send(ClickedOutside{});
    expect(mode(island.state) != Mode::Expanded && !island.registry.nextWake() && island.content() == overviewContent({}) &&
           island.stats->page() == StatsPage::Overview, "closed: no more wake-ups, an empty overview for next time");
    island.stats->publish(fullReading());
    expect(island.content() == overviewContent({}) && island.asked == 2, "a slow read's answer after closing is dropped");
    island.send(PillClicked{});
    island.stats->publish(fullReading());
    expect(island.asked == 3 && std::get<Graph>(island.content().expandedCenter[0].items[0]).samples.size() == 1, "opened again: asking again, histories from scratch");

    // Beside another page: its card is the height the timer's is.
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    island.send(CardOpened{ StatisticsProvider::kPanelId });
    float statsHeight = island.scene().size.y;
    island.click(stats_action::kOpenBluetooth);
    expect(island.scene().size.y == statsHeight && island.scene().tabs.has_value(), "a page under the page row: the same height");
    island.send(CardOpened{ PomodoroProvider::kPanelId });
    expect(island.scene().size.y == statsHeight && statsHeight == t.expandedSize.y + tabStripSpace(island.state), "switching to the timer doesn't resize the island");
    expect(!island.registry.nextWake() && island.asked == 3 && island.stats->page() == StatsPage::Overview, "the timer's page shown: statistics stopped asking");

    island.registry.stop("statistics");
    expect(island.state.panels.size() == 1 && island.state.panels[0].id == PomodoroProvider::kPanelId, "stopped: its page is gone");
    island.stats->publish(fullReading()); // must do nothing, and not crash
    island.stats->onAction({ "statistics", StatisticsProvider::kPanelId, stats_action::kOpenCpu, 0 });
}

} // namespace

void runStatisticsProviderTests() { providerTests(); }

} // namespace cup::test
