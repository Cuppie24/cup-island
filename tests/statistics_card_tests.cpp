// The statistics card's overview (decision 172): its tiles, laid out by the
// real layout() at the height every card shares, each a button for its page.
// The pages are in statistics_page_tests.cpp, the provider in statistics_provider_tests.cpp.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/stat_parts.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/media/now_playing.h"
#include "providers/statistics/stats_card.h"
#include "statistics_fixture.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace cup::test {
namespace {

float mediaHeight()
{
    IslandState media;
    NowPlaying song{ .title = "Song", .artist = "Artist", .timeline = { .duration = Ms{ 200000 }, .playing = true } };
    media = reduce(media, ActivityStarted{ { .id = "media", .content = nowPlayingContent(song) } }, Ms{ 0 });
    media = reduce(media, PillClicked{}, Ms{ 0 });
    return layout(media, Ms{ 0 }).size.y;
}

void overviewTests()
{
    const Tokens& t = tokens();
    StatsView view = viewOf(fullReading());
    Content card = overviewContent(view);
    const Slot& rows = card.expandedCenter;
    expect(card.card == CardLayout::Centred && card.noIdleTimeout && card.keepOpen && !card.accent && card.backAction.empty() && rows.size() == 2 &&
           rows[0].items.size() == 3 && rows[1].items.size() == 3 && rows[0].even && rows[1].even && rows[1].tileGap, "two rows of three tiles");
    expect(card.compactCenter.empty() && card.minimal.empty(), "a page only: nothing for the pill");
    const Graph& cpu = std::get<Graph>(rows[0].items[0]);
    const Graph& gpu = std::get<Graph>(rows[0].items[1]);
    const Graph& ram = std::get<Graph>(rows[0].items[2]);
    expect(cpu.tile && cpu.size == GraphSize::Tall && cpu.caption == "CPU" && cpu.detail == "73°" && cpu.value == "58%" && cpu.valueLarge &&
           cpu.actionId == stats_action::kOpenCpu && !cpu.alert && !cpu.limit, "CPU: its temperature beside its name, its load big");
    expect(cpu.samples == std::vector<float>{ 0.58f, 0.58f, 0.58f } && cpu.capacity == t.statsHistoryLength, "its history, growing in from the right");
    expect(gpu.caption == "GPU" && gpu.detail == "50°" && gpu.value == "29%" && gpu.actionId == stats_action::kOpenGpu, "GPU");
    expect(ram.caption == "RAM" && ram.detail.empty() && ram.value == "58%" && ram.actionId == stats_action::kOpenMemory, "RAM: no room for gigabytes beside three");
    const Graph& net = std::get<Graph>(rows[1].items[0]);
    expect(net.tile && net.size == GraphSize::Low && net.caption == "↓ 717 ↑ 307 KB/s" && net.captionCentred && net.value.empty() &&
           net.actionId == stats_action::kOpenNetwork && net.samples.size() == 3, "network: both speeds over the download's history");
    const LevelTile& disk = std::get<LevelTile>(rows[1].items[1]);
    expect(disk.glyph == Glyph::Drive && disk.caption == "Disk C:" && disk.value == "71%" && std::abs(disk.level - 0.7109f) < 0.001f &&
           disk.detail == "148 GB free" && !disk.alert && disk.actionId == stats_action::kOpenDisk, "disk: the system drive's level and what's free");
    const BatteryTile& bt = std::get<BatteryTile>(rows[1].items[2]);
    expect(bt.cells.size() == 3 && bt.cells[0] == BatteryCell{ Glyph::Keyboard, 0.45f, false, "45%" } && bt.cells[1].glyph == Glyph::Mouse &&
           bt.cells[2].glyph == Glyph::Headphones && bt.actionId == stats_action::kOpenBluetooth, "Bluetooth: glyph, battery, percent, the emptiest first");
    expect(driveCaption("Windows (C:)") == "Disk C:" && driveCaption("Backup") == "Backup" && driveCaption("") == "Disk" && driveCaption("(C:") == "(C:",
           "a drive's letter from its label");

    // Alerts mark only the tile concerned: no accent on the card.
    StatsReading hot = fullReading();
    hot.cpuTemperature = static_cast<float>(t.statsHighTemperature) + 2, hot.gpuLoad = 97, hot.memoryUsed = hot.memoryTotal;
    hot.drives[0].used = hot.drives[0].total, hot.bluetooth[0].battery = static_cast<float>(t.statsLowBattery), hot.networkDown = 12.4f * 1000 * 1000;
    Content alarmed = overviewContent(viewOf(hot));
    const Slot& hotRows = alarmed.expandedCenter;
    expect(!alarmed.accent && std::get<Graph>(hotRows[0].items[0]).alert && std::get<Graph>(hotRows[0].items[1]).alert && std::get<Graph>(hotRows[0].items[2]).alert,
           "a hot CPU, a busy GPU, full memory: each its own tile");
    expect(std::get<Graph>(hotRows[1].items[0]).alert && std::get<LevelTile>(hotRows[1].items[1]).alert, "a download at the link's limit, a full disk");
    const BatteryTile& lowTile = std::get<BatteryTile>(hotRows[1].items[2]);
    expect(lowTile.cells[0].alert && lowTile.cells[0].glyph == Glyph::Mouse && !lowTile.cells[1].alert, "a low device: first, and marked; the others not");
    StatsReading oneHot = fullReading();
    oneHot.cpuLoad = 99;
    const Slot oneRows = overviewRows(viewOf(oneHot));
    expect(std::get<Graph>(oneRows[0].items[0]).alert && !std::get<Graph>(oneRows[0].items[1]).alert && !std::get<Graph>(oneRows[0].items[2]).alert,
           "one high reading no longer turns the others red");

    // Sparse: no GPU, no temperatures, no drives, no devices, nothing read at all.
    StatsReading sparse{ .cpuLoad = 5, .memoryUsed = kGb * 93 / 10, .memoryTotal = 16 * kGb };
    const Slot few = overviewRows(viewOf(sparse));
    expect(few[0].items.size() == 2 && std::get<Graph>(few[0].items[0]).detail.empty() && std::get<Graph>(few[0].items[1]).caption == "RAM" &&
           std::get<Graph>(few[0].items[1]).detail == "9.3 GB", "no GPU: two tiles on top, RAM with its gigabytes");
    expect(few[1].items.size() == 2 && std::get<Graph>(few[1].items[0]).caption == std::string("Network ") + kNoReading &&
           std::get<BatteryTile>(few[1].items[1]).cells.empty() && std::get<BatteryTile>(few[1].items[1]).empty == "No devices" &&
           std::get<BatteryTile>(few[1].items[1]).actionId == stats_action::kOpenBluetooth, "no drives: no disk tile; no devices: the tile says so, and still opens");
    const Slot bare = overviewRows({});
    expect(bare[0].items.size() == 2 && std::get<Graph>(bare[0].items[0]).value == kNoReading && std::get<Graph>(bare[0].items[0]).samples.empty(),
           "nothing read yet: the same tiles, a dash, an empty graph");
    StatsReading many = fullReading();
    for (int i = 0; i < 4; ++i) many.bluetooth.push_back({ "Extra " + std::to_string(i), 99.0f });
    expect(std::get<BatteryTile>(overviewRows(viewOf(many))[1].items[2]).cells.size() == t.statsBluetoothShown, "more devices than fit: the emptiest few");

    // Laid out: the height every card shares, each tile a button for its page.
    float media = mediaHeight();
    for (const StatsView& v : { StatsView{}, viewOf(sparse), view, viewOf(hot), viewOf(many) }) {
        Scene scene = shown(v);
        expect(scene.size.y == media && scene.size.y == t.expandedSize.y && scene.size.x == t.expandedSize.x,
               "media's and the timer's card size (" + std::to_string(int(scene.size.y)) + ")");
        expectFits(scene, "overview");
    }
    Scene scene = shown(view);
    const char* actions[] = { stats_action::kOpenCpu, stats_action::kOpenGpu, stats_action::kOpenMemory,
                              stats_action::kOpenNetwork, stats_action::kOpenDisk, stats_action::kOpenBluetooth };
    for (int i = 0; i < 6; ++i) {
        const Placed* tile = nullptr;
        for (const Placed& p : scene.elements)
            if (p.key == "expandedCenter/" + std::to_string(i / 3) + "/" + std::to_string(i % 3)) tile = &p;
        expect(tile != nullptr, "a tile is placed");
        if (!tile) continue;
        // Its middle, and just inside each corner: the whole tile is the button.
        for (Vec2 at : { Vec2{ tile->frame.x + tile->frame.width / 2, tile->frame.y + tile->frame.height / 2 }, Vec2{ tile->frame.x + 1, tile->frame.y + 1 },
                         Vec2{ tile->frame.x + tile->frame.width - 1, tile->frame.y + tile->frame.height - 1 } }) {
            auto hit = hitTest(scene, at);
            expect(hit && hit->action.actionId == actions[i] && hit->action.activityId == kStatisticsPanelId, std::string("a click on a tile opens its page: ") + actions[i]);
        }
        expect(pageOpenedBy(actions[i]).has_value() && std::string(openAction(*pageOpenedBy(actions[i]))) == actions[i], "its action names its page");
    }
    expect(!pageOpenedBy(stats_action::kBack) && !pageOpenedBy("") && !pageOpenedBy("open.else") && std::string(openAction(StatsPage::Overview)).empty(),
           "nothing else opens a page");
    const Placed* first = all<Graph>(scene)[0];
    const Placed* under = all<Graph>(scene)[3];
    expect(first->frame.height == t.graphTallHeight && under->frame.height == t.graphLowHeight &&
           std::abs(under->frame.y - first->frame.y - first->frame.height - t.itemSpacing) < 0.01f, "the mockup's grid: 124 over 80, one gap");
}

} // namespace

void runStatisticsCardTests()
{
    overviewTests();
}

} // namespace cup::test
