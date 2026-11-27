// The statistics card and pill as content (decision 174): each tile, sparse
// or full, the grid laid out by the real layout() at the height every card
// shares, and the pill's slots. The provider is in statistics_provider_tests.cpp.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/slot_layout.h"
#include "core/stat_parts.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/media/now_playing.h"
#include "statistics_fixture.h"

#include <cmath>

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

void tileTests()
{
    const Tokens& t = tokens();
    StatsReading r = fullReading();
    expect(cpuTile(r) == StatTile{ .glyph = Glyph::Chip, .caption = "CPU", .detail = "46°", .value = "14%", .level = 0.14f, .large = true },
           "CPU: its temperature at the right, its load big over its level");
    expect(gpuTile(r) == StatTile{ .glyph = Glyph::GraphicsCard, .caption = "GPU", .detail = "42°", .value = "13%", .level = 0.13f, .large = true }, "GPU");
    StatTile ram = memoryTile(r, false);
    expect(ram.glyph == Glyph::Memory && ram.caption == "RAM" && ram.detail == "16.5 GB" && ram.value == "52%" && std::abs(*ram.level - 0.5156f) < 0.001f &&
           ram.large && !ram.alert && memoryTile(r, true).detail == "16.5 / 32 GB", "RAM: what's used; with room, of how much");
    StatTile disk = diskTile(r);
    expect(disk.glyph == Glyph::Drive && disk.caption == "Disk" && disk.value == "71%" && std::abs(*disk.level - 0.7109f) < 0.001f && !disk.large &&
           disk.detail.empty() && !disk.alert, "disk: the system drive's level, a small tile");
    StatTile net = networkTile(r);
    expect(net.glyph == Glyph::Network && net.caption.empty() && net.value == "↓ 9.9 ↑ 0.3 MB/s" && !net.level && !net.large && !net.alert,
           "network: both speeds on one line, no level, no name beside them");

    // What's missing: a dash, an empty level; the CPU's cores where there's no temperature.
    StatsReading sparse{ .cpuLoad = 40, .cpuCores = 6, .memoryUsed = kGb * 87 / 10, .memoryTotal = 16 * kGb };
    expect(cpuTile(sparse).detail == "6 cores" && cpuTile({ .cpuLoad = 1, .cpuCores = 1 }).detail == "1 core" && cpuTile({ .cpuLoad = 1 }).detail.empty() &&
           cpuTile({ .cpuLoad = 1, .cpuTemperature = 50, .cpuCores = 8 }).detail == "50°", "no temperature: how many cores, if known");
    StatTile blank = cpuTile({});
    expect(blank.value == kNoReading && blank.level == 0.0f && blank.detail.empty() && !blank.alert && blank.large, "nothing read: a dash over an empty level");
    expect(memoryTile({}, true).detail.empty() && diskTile({}).value == kNoReading && diskTile({}).level == 0.0f && networkTile({}).value == kNoReading,
           "...on every tile, each keeping its place");
    expect(hasGpu(r) && !hasGpu(sparse) && !hasGpu({ .gpuLoad = std::nanf("") }) && hasGpu({ .gpuLoad = 0 }), "a GPU tile only with a GPU reading");

    // High, hot and low mark only their own tile.
    StatsReading hot = r;
    hot.cpuTemperature = static_cast<float>(t.statsHighTemperature), hot.gpuLoad = 97, hot.memoryUsed = hot.memoryTotal, hot.diskUsed = hot.diskTotal;
    expect(cpuTile(hot).alert && gpuTile(hot).alert && memoryTile(hot, false).alert && diskTile(hot).alert && !networkTile(hot).alert &&
           !statisticsContent(viewOf(hot)).accent, "a hot CPU, a busy GPU, full memory, a full disk: each its own tile, no accent on the card");
    StatsReading one = r;
    one.cpuLoad = 99;
    expect(cpuTile(one).alert && !gpuTile(one).alert && !memoryTile(one, false).alert, "one high reading doesn't mark the others");

    // Bluetooth: the emptiest first, as many as fit.
    BatteryTile three = bluetoothTile(fullDevices());
    expect(three.note.empty() && three.rows.size() == 3 && three.rows[0] == BatteryRow{ Glyph::Keyboard, "Keychron K3", 0.45f, false, "45%" } &&
           three.rows[1].glyph == Glyph::Mouse && three.rows[2].glyph == Glyph::Headphones, "a row per device: its glyph, name, level and percent");
    std::vector<BluetoothDevice> five = fullDevices();
    five.push_back({ "Pen", 5.0f, DeviceKind::Pen }), five.push_back({ "Thing", static_cast<float>(t.statsLowBattery), DeviceKind::Other });
    BatteryTile many = bluetoothTile(five);
    expect(many.rows.size() == 2 && many.note == "+3 more" && many.rows[0].name == "Pen" && many.rows[0].alert && many.rows[0].glyph == Glyph::Pen &&
           many.rows[1].name == "Thing" && many.rows[1].alert && many.rows[1].glyph == Glyph::Bluetooth,
           "more than fit: the emptiest, low ones marked, and how many more");
    BatteryTile alone = bluetoothTile({ { "Galaxy Buds2 Pro", 74.0f, DeviceKind::Earbuds }, { "Speaker", std::nullopt } });
    expect(alone.rows.size() == 1 && alone.note.empty() && alone.rows[0].glyph == Glyph::Earbuds && alone.rows[0].value == "74%", "one device: the tile to itself");
    expect(bluetoothTile({}) == BatteryTile{ .note = "No devices connected" } && bluetoothTile({ { "Speaker", std::nullopt } }).rows.empty(),
           "none with a level: the tile says so, and stays");
}

void cardTests()
{
    const Tokens& t = tokens();
    StatsView view = viewOf(fullReading());
    Content card = statisticsContent(view);
    expect(card.card == CardLayout::Columns && card.keepOpen && card.noIdleTimeout && !card.accent && !card.wideCard && card.backAction.empty() &&
           card.sizedLike.empty() && card.label == "System", "one card: no pages, the usual width, kept in view");
    expect(card.expandedCenter.size() == 1 && card.expandedCenter[0].even && card.expandedCenter[0].items.size() == 3 &&
           std::get<StatTile>(card.expandedCenter[0].items[0]) == cpuTile(view.reading) && std::get<StatTile>(card.expandedCenter[0].items[1]) == gpuTile(view.reading) &&
           std::get<StatTile>(card.expandedCenter[0].items[2]) == memoryTile(view.reading, false), "the top row: CPU, GPU, RAM");
    expect(card.expandedLeading.size() == 2 && std::get<StatTile>(card.expandedLeading[0].items[0]) == diskTile(view.reading) &&
           std::get<StatTile>(card.expandedLeading[1].items[0]) == networkTile(view.reading) && card.expandedLeading[1].tileGap && !card.expandedLeading[0].tileGap,
           "under it at the left: the disk over the network");
    expect(card.expandedTrailing.size() == 1 && std::get<BatteryTile>(card.expandedTrailing[0].items[0]) == bluetoothTile(fullDevices()), "at the right: Bluetooth");
    expect(!hasPill(card) && card.compactLeading.empty() && card.compactCenter.empty() && card.minimal.empty(), "nothing picked: nothing for the pill");

    StatsReading sparse{ .cpuLoad = 40, .cpuCores = 6, .memoryUsed = kGb * 87 / 10, .memoryTotal = 16 * kGb };
    Content few = statisticsContent(viewOf(sparse));
    expect(few.expandedCenter[0].items.size() == 2 && std::get<StatTile>(few.expandedCenter[0].items[1]).detail == "8.7 / 16 GB" &&
           few.expandedLeading.size() == 2 && std::get<BatteryTile>(few.expandedTrailing[0].items[0]).note == "No devices connected",
           "no GPU: two tiles on top, RAM with used of total; the rest keeps its place");

    // Laid out: the height every card shares, every tile inside the padding, none over another.
    float media = mediaHeight();
    std::vector<BluetoothDevice> crowd = fullDevices();
    for (int i = 0; i < 6; ++i) crowd.push_back({ "A device with a very long name " + std::to_string(i), 10.0f + static_cast<float>(i) });
    StatsReading crowded = fullReading(), single = fullReading();
    crowded.bluetooth = crowd;
    single.bluetooth = std::vector<BluetoothDevice>{ { "Galaxy Buds2 Pro", 74.0f, DeviceKind::Earbuds } };
    for (const StatsView& v : { StatsView{}, viewOf(sparse), view, viewOf(crowded), viewOf(single) }) {
        Scene scene = shown(v);
        expect(scene.size.y == media && scene.size.y == t.expandedSize.y && scene.size.x == t.expandedSize.x,
               "media's and the timer's card size (" + std::to_string(int(scene.size.y)) + ")");
        std::vector<const Placed*> tiles;
        for (const Placed& p : scene.elements)
            if (p.key.rfind("expanded", 0) == 0) tiles.push_back(&p);
        expect(tiles.size() == (hasGpu(v.reading) ? 6u : 5u), "every tile is placed");
        for (const Placed* a : tiles) {
            const Rect& f = a->frame;
            expect(f.x >= t.expandedPadding - 0.01f && f.x + f.width <= scene.size.x - t.expandedPadding + 0.01f && f.y >= t.expandedPadding - 0.01f &&
                   f.y + f.height <= scene.size.y - t.expandedPadding + 0.01f && f.width > 0 && f.height > 0, "fits its card: " + a->key);
            expect(!hitTest(scene, { f.x + f.width / 2, f.y + f.height / 2 }), "a tile opens nothing: " + a->key);
            for (const Placed* b : tiles) {
                if (a == b) continue;
                bool apart = f.x + f.width <= b->frame.x + 0.01f || b->frame.x + b->frame.width <= f.x + 0.01f || f.y + f.height <= b->frame.y + 0.01f ||
                             b->frame.y + b->frame.height <= f.y + 0.01f;
                expect(apart, "tiles don't overlap: " + a->key + " / " + b->key);
            }
        }
    }
    // The mockup's grid: 100 over 104, the Bluetooth tile as tall as disk and network together.
    Scene scene = shown(view);
    const Placed* cpu = placedAt(scene, "expandedCenter/0/0");
    const Placed* disk = placedAt(scene, "expandedLeading/0/0");
    const Placed* net = placedAt(scene, "expandedLeading/1/0");
    const Placed* bt = placedAt(scene, "expandedTrailing/0/0");
    expect(cpu && disk && net && bt && cpu->frame.height == t.statTileHeight && disk->frame.height == t.statMiniHeight &&
           std::abs(bt->frame.height - (net->frame.y + net->frame.height - disk->frame.y)) < 0.01f &&
           std::abs(disk->frame.y - cpu->frame.y - cpu->frame.height - t.itemSpacing) < 0.01f && std::abs(bt->frame.width - disk->frame.width) < 0.01f,
           "the grid: a row of three, then two small tiles beside a tall one, one gap everywhere");
    // The network's line fits its tile with the real numbers' widest ("999 KB/s" each way).
    StatTile widest = networkTile({ .networkDown = 999.0f * 1024, .networkUp = 999.0f * 1024 });
    StatTileParts parts = statTileParts(widest, disk->frame, {});
    expect(parts.aside.x >= parts.glyph.x + parts.glyph.width && parts.aside.x + parts.aside.width <= disk->frame.x + disk->frame.width - t.tileInset + 0.01f,
           "both speeds fit beside the glyph, at their widest");
}

void pillTests()
{
    const Tokens& t = tokens();
    namespace pick = stats_pick;
    StatsView view = viewOf(fullReading(), { .picked = { pick::kCpuLoad, pick::kCpuTemperature } });
    Content content = statisticsContent(view);
    expect(hasPill(content) && std::get<Icon>(content.compactLeading[0].items[0]).glyph == Glyph::Gauge &&
           std::get<Icon>(content.minimal[0].items[0]).glyph == Glyph::Gauge && content.compactTrailing.empty(), "the pill: a gauge, and in the bubble too");
    const Text& line = std::get<Text>(content.compactCenter[0].items[0]);
    expect(line.value == "CPU 14% · 46°" && line.brightDigits && line.marquee && line.centred && line.style == TextStyle::Body,
           "its line: the picked readings, numbers bright; it swims if it's ever too long");
    expect(content.expandedCenter == statisticsContent(viewOf(fullReading())).expandedCenter, "the card is the same with or without a pill");
    // Picked, but nothing to show: no pill.
    expect(!hasPill(statisticsContent(viewOf({}, view.picks))) && !hasPill(statisticsContent(viewOf(fullReading(), { .picked = { "bt:Gone" } }))),
           "no reading for what's picked: no pill");

    // Laid out as the pill: the gauge at its left, the line inside, two readings fitting without swimming.
    auto compact = [](const StatsView& v) {
        IslandState state;
        state = reduce(state, PanelPublished{ statisticsPanel(v) }, Ms{ 0 });
        state = reduce(state, ActivityStarted{ { .id = kStatisticsPanelId, .level = Level::Passive, .content = statisticsContent(v) } }, Ms{ 0 });
        return layout(state, Ms{ 0 });
    };
    Scene pill = compact(view);
    const Placed* icon = placedAt(pill, "compactLeading/0/0");
    const Placed* text = placedAt(pill, "compactCenter/0/0");
    expect(pill.mode == Mode::Compact && pill.size.x == t.compactSize.x && icon && text && icon->frame.x < text->frame.x &&
           text->frame.x + text->frame.width <= pill.size.x && text->frame.width > 0, "a compact pill: the gauge, then the line");
    if (!text) return;
    // Every pair of the fixed readings fits the line's room (the rough text measure tests use); only a long device name may swim.
    const char* ids[] = { pick::kCpuLoad, pick::kCpuTemperature, pick::kGpuLoad, pick::kMemory, pick::kDisk };
    StatsReading busy = fullReading();
    busy.cpuLoad = 100, busy.gpuLoad = 100, busy.memoryUsed = busy.memoryTotal, busy.diskUsed = busy.diskTotal, busy.cpuTemperature = 100;
    for (const char* a : ids)
        for (const char* b : ids) {
            std::string value = pillText(busy, {}, { .picked = { a, b } });
            expect(estimateTextWidth(Text{ value }) <= text->frame.width, "two readings fit the pill: " + value);
        }
}

} // namespace

void runStatisticsCardTests()
{
    tileTests();
    cardTests();
    pillTests();
}

} // namespace cup::test
