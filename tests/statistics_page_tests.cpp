// The statistics card's pages (decision 172): what each shows, sparse or
// full, and that each fits its card, wide (CPU, GPU, memory, disk) or at the
// usual width (network, Bluetooth), with its back arrow at the top left.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "statistics_fixture.h"

#include <cmath>

namespace cup::test {
namespace {

void widePageTests()
{
    const Tokens& t = tokens();
    StatsReading full = fullReading();

    // CPU: load and temperature side by side, the cores under each, the processes at the right.
    Content cpu = cpuPage(viewOf(full, StatsPage::Cpu));
    expect(cpu.card == CardLayout::Columns && cpu.wideCard && cpu.backAction == stats_action::kBack && cpu.keepOpen && cpu.noIdleTimeout &&
           cpu.sizedLike == overviewRows(viewOf(full, StatsPage::Cpu)) && cpu.label == "CPU", "a wide page of the card, the overview's height, Esc goes back");
    const Row& header = cpu.expandedCenter[0];
    expect(cpu.expandedCenter.size() == 1 && std::get<Text>(header.items[0]).value == "CPU" && header.hug && header.align == Align::Center &&
           std::get<Button>(header.leading[0]).actionId == stats_action::kBack && std::get<Button>(header.leading[0]).glyph == Glyph::ChevronLeft &&
           std::get<Text>(header.trailing[0]).value == "AMD Ryzen 7 7840U", "its header: back, the title centred, the CPU's name");
    const Slot& left = cpu.expandedLeading;
    expect(left.size() == 4, "names, numbers, histories, cores");
    for (const Row& r : left) expect(r.even && r.loose && r.items.size() == 2, "two equal columns");
    expect(std::get<Text>(left[0].items[0]).value == "Load · 8 cores" && std::get<Text>(left[0].items[1]).value == "Temperature", "what each is");
    expect(std::get<Text>(left[1].items[0]) == Text{ .value = "58%", .style = TextStyle::Hero } && std::get<Text>(left[1].items[1]).value == "73°", "big numbers");
    const Graph& load = std::get<Graph>(left[2].items[0]);
    const Graph& temp = std::get<Graph>(left[2].items[1]);
    expect(load.size == GraphSize::Strip && !load.tile && !load.limit && load.actionId.empty() && load.samples.size() == 3 && !load.alert, "the load's history, bare");
    expect(temp.limit == temperatureLimit() && std::abs(temp.samples[0] - temperatureSamples({ 73.0f })[0]) < 0.0001f && !temp.alert,
           "the temperature's, with its limit line");
    const Levels& cores = std::get<Levels>(left[3].items[0]);
    const Levels& coreTemps = std::get<Levels>(left[3].items[1]);
    expect(cores.bars.size() == 8 && cores.bars[2] == LevelBar{ 0.95f, true, "Core 3 · 95%" } && !cores.bars[0].alert && cores.bars[0].label == "Core 1 · 40%",
           "a bar per core, a pegged one marked, each with its label");
    expect(coreTemps.bars.size() == 8 && coreTemps.bars[2].alert && coreTemps.bars[2].label == "Core 3 · 88°" && !coreTemps.bars[1].alert &&
           coreTemps.bars[1].level == temperatureSamples({ 72.0f })[0], "a temperature per core, on the graph's scale");
    const Slot& right = cpu.expandedTrailing;
    expect(right.size() == 1 + t.statsListShown && std::get<Text>(right[0].items[0]).value == "Top processes", "the top processes");
    expect(std::get<Text>(right[1].items[0]).value == "Chrome" && std::get<Progress>(right[1].trailing[0]) == Progress{ .level = 1.0f, .stub = true } &&
           std::get<Text>(right[1].trailing[1]).value == "11.6%" && std::get<Text>(right[2].items[0]).value == "Visual Studio Code" &&
           std::abs(*std::get<Progress>(right[2].trailing[0]).level - 5.6f / 11.6f) < 0.001f && std::get<Text>(right[5].items[0]).value == "Explorer",
           "the biggest first, its bar full, the others against it");
    StatsReading hot = full;
    hot.cpuLoad = 97, hot.cpuTemperature = 91;
    const Slot hotLeft = cpuPage(viewOf(hot, StatsPage::Cpu)).expandedLeading;
    expect(std::get<Graph>(hotLeft[2].items[0]).alert && std::get<Graph>(hotLeft[2].items[1]).alert && !cpuPage(viewOf(hot)).accent, "high: its graph, not the card");
    // No temperature at all: one column, and it says why.
    StatsReading cool = full;
    cool.cpuTemperature.reset(), cool.coreTemperatures.clear(), cool.cpuProcesses.clear(), cool.cpuName.clear();
    Content plain = cpuPage(viewOf(cool, StatsPage::Cpu));
    expect(plain.expandedLeading.size() == 4 && plain.expandedLeading[1].items.size() == 2 && std::get<Text>(plain.expandedLeading[1].items[1]).value == kNoReading &&
           std::get<Text>(plain.expandedLeading[2].items[1]).value == "Isn't reported on this PC" && std::holds_alternative<Graph>(plain.expandedLeading[2].items[0]) &&
           std::holds_alternative<Levels>(plain.expandedLeading[3].items[0]) && std::get<Text>(plain.expandedLeading[3].items[1]).value.empty() &&
           plain.expandedTrailing.empty(), "no temperature: its column keeps its place and says so; no processes: no list");
    StatsView lost = viewOf(full, StatsPage::Cpu);
    lost.reading.cpuTemperature.reset();
    expect(std::holds_alternative<Graph>(cpuPage(lost).expandedLeading[2].items[1]) && std::get<Text>(cpuPage(lost).expandedLeading[1].items[1]).value == kNoReading,
           "a temperature that stops being read: a dash over the history it had");
    StatsReading noCores = full;
    noCores.coreLoads.clear();
    const Slot fewer = cpuPage(viewOf(noCores, StatsPage::Cpu)).expandedLeading;
    expect(fewer.size() == 4 && std::get<Text>(fewer[0].items[0]).value == "Load" && std::holds_alternative<Text>(fewer[3].items[0]) &&
           std::holds_alternative<Levels>(fewer[3].items[1]), "no per-core load: its column's place stays empty");
    noCores.coreTemperatures.clear();
    expect(cpuPage(viewOf(noCores, StatsPage::Cpu)).expandedLeading.size() == 3, "no cores at all: no strips");

    // GPU: the CPU page's shape, dedicated memory where the cores are, engines and processes at the right.
    Content gpu = gpuPage(viewOf(full, StatsPage::Gpu));
    const Slot& gleft = gpu.expandedLeading;
    expect(gpu.card == CardLayout::Columns && gpu.wideCard && gleft.size() == 4 && std::get<Text>(gleft[1].items[0]).value == "29%" &&
           std::get<Text>(gleft[1].items[1]).value == "50°" && std::get<Graph>(gleft[2].items[1]).limit.has_value() &&
           std::get<Text>(gpu.expandedCenter[0].trailing[0]).value == "GeForce RTX 4060", "load and temperature, like the CPU's");
    expect(std::get<Text>(gleft[3].leading[0]).value == "Memory" && std::abs(*std::get<Progress>(gleft[3].items[0]).level - 3.5f / 8) < 0.001f &&
           std::get<Text>(gleft[3].trailing[0]).value == "3.5 / 8 GB", "dedicated memory under both");
    const Slot& gright = gpu.expandedTrailing;
    expect(gright.size() == 8 && std::get<Text>(gright[0].items[0]).value == "Engines" && std::get<Text>(gright[1].items[0]).value == "3D" &&
           std::abs(*std::get<Progress>(gright[1].trailing[0]).level - 0.47f) < 0.001f && std::get<Text>(gright[2].items[0]).value == "Video encode" &&
           std::get<Text>(gright[4].items[0]).value == "Top processes" && std::get<Text>(gright[5].items[0]).value == "Chrome" &&
           *std::get<Progress>(gright[5].trailing[0]).level == 1.0f, "three engines (their own percent), three processes (against the biggest)");
    StatsReading integrated{ .gpuLoad = 12 };
    Content small = gpuPage(viewOf(integrated, StatsPage::Gpu));
    expect(small.expandedLeading.size() == 3 && small.expandedLeading[0].items.size() == 2 && small.expandedTrailing.empty() &&
           std::get<Text>(small.expandedLeading[2].items[1]).value == "Isn't reported on this PC", "a GPU that only reports its load");

    // Memory: the history, in use / cached / free as one bar, the apps.
    Content mem = memoryPage(viewOf(full, StatsPage::Memory));
    const Graph& used = std::get<Graph>(mem.expandedLeading[0].items[0]);
    const Progress& parts = std::get<Progress>(mem.expandedLeading[1].items[0]);
    expect(mem.card == CardLayout::Columns && used.tile && used.size == GraphSize::Tall && used.caption == "In use" && used.value == "18.6 GB · 58%" &&
           used.valueLarge && std::get<Text>(mem.expandedCenter[0].trailing[0]).value == "32 GB", "in use, as gigabytes and percent, of the total");
    expect(parts.thick && std::abs(*parts.level - 18.6f / 32) < 0.001f && std::abs(*parts.second - 6.1f / 32) < 0.001f && !parts.alert, "one bar: in use, then cached");
    const Row& legend = mem.expandedLeading[2];
    expect(legend.even && legend.items.size() == 3 && std::get<Text>(legend.items[0]).value == "Used 18.6 GB" &&
           std::get<Text>(legend.items[1]).value == "Cached 6.1 GB" && std::get<Text>(legend.items[2]).value == "Free 7.3 GB", "and what each part is");
    expect(mem.expandedTrailing.size() == 5 && std::get<Text>(mem.expandedTrailing[1].trailing[1]).value == "4.6 GB" &&
           std::get<Text>(mem.expandedTrailing[4].trailing[1]).value == "700 MB", "the apps holding it");
    StatsReading odd = full;
    odd.memoryCached = 64 * kGb, odd.memoryUsed = 31 * kGb;
    const Progress& capped = std::get<Progress>(memoryPage(viewOf(odd)).expandedLeading[1].items[0]);
    expect(capped.alert && *capped.level + *capped.second <= 1.0001f, "cached never past what's left; nearly full is marked");
    odd.memoryCached.reset();
    expect(memoryPage(viewOf(odd)).expandedLeading[2].items.size() == 2 && !std::get<Progress>(memoryPage(viewOf(odd)).expandedLeading[1].items[0]).second,
           "no cached reading: used and free");
    expect(memoryPage({}).expandedLeading.size() == 1 && std::get<Graph>(memoryPage({}).expandedLeading[0].items[0]).value == kNoReading, "nothing read: the empty graph");

    // Disk: every drive, and how hard they're working.
    Content disk = diskPage(viewOf(full, StatsPage::Disk));
    expect(disk.expandedLeading.size() == 6 && std::get<Text>(disk.expandedCenter[0].trailing[0]).value == "3 drives", "a name row and a bar per drive");
    expect(std::get<Text>(disk.expandedLeading[0].items[0]).value == "Windows (C:)" && std::get<Text>(disk.expandedLeading[0].trailing[0]).value == "364 / 512 GB" &&
           std::get<Text>(disk.expandedLeading[0].trailing[1]).value == "71%" && !disk.expandedLeading[0].apart && disk.expandedLeading[2].apart &&
           std::get<Text>(disk.expandedLeading[4].trailing[0]).value == "1.28 / 1.95 TB", "its name, used of total, percent");
    expect(std::get<Graph>(disk.expandedTrailing[0].items[0]).caption == "Read" && std::get<Graph>(disk.expandedTrailing[0].items[0]).value == "8 MB/s" &&
           std::get<Graph>(disk.expandedTrailing[1].items[0]).caption == "Write" && disk.expandedTrailing[1].tileGap, "read and write, a tile each");
    StatsReading packed = full;
    packed.diskRead.reset(), packed.diskWrite.reset();
    packed.drives[0].used = packed.drives[0].total;
    for (int i = 0; i < 6; ++i) packed.drives.push_back({ "Extra", 1 * kGb, 2 * kGb });
    Content crowded = diskPage(viewOf(packed, StatsPage::Disk));
    expect(crowded.expandedLeading.size() == 2 * t.statsDrivesShown && !crowded.expandedLeading[2].apart && crowded.expandedTrailing.empty() &&
           std::get<Progress>(crowded.expandedLeading[1].items[0]).alert && !std::get<Progress>(crowded.expandedLeading[3].items[0]).alert &&
           std::get<Text>(crowded.expandedCenter[0].trailing[0]).value == "9 drives", "many drives: the first few, closer; a full one marked; no activity: no graphs");

    // Every wide page fits a wide card of the overview's height, sparse or full.
    for (const StatsReading& r : { full, hot, cool, noCores, integrated, odd, packed, StatsReading{ .cpuLoad = 1, .drives = { { "", 1, 2 } } } })
        for (StatsPage page : { StatsPage::Cpu, StatsPage::Gpu, StatsPage::Memory, StatsPage::Disk }) {
            if (!hasPage(r, page)) continue;
            for (int readings : { 0, 60 }) {
                Scene scene = shown(viewOf(r, page, readings));
                std::string what = std::string(openAction(page)) + (readings ? "" : " (no history)");
                expect(scene.size.x == t.wideCardWidth && scene.size.y == t.expandedSize.y, what + ": wide, the overview's height");
                expectFits(scene, what);
                auto back = hitTest(scene, { t.expandedPadding + 4, t.expandedPadding + 4 });
                expect(back && back->action.actionId == stats_action::kBack, what + ": the back arrow at its top left");
            }
        }
    Scene scene = shown(viewOf(full, StatsPage::Cpu));
    const Placed* title = textNamed(scene, "CPU");
    expect(title && std::abs(title->frame.x + title->frame.width / 2 - scene.size.x / 2) < 1, "the title in the card's middle");
    auto strips = all<Levels>(scene);
    auto graphs = all<Graph>(scene);
    expect(strips.size() == 2 && graphs.size() == 2 && strips[0]->frame.x == graphs[0]->frame.x && strips[0]->frame.width == graphs[0]->frame.width &&
           strips[1]->frame.x == graphs[1]->frame.x && graphs[1]->frame.x > graphs[0]->frame.x + graphs[0]->frame.width, "the cores under their history, column by column");
    const Placed* process = textNamed(scene, "Chrome");
    expect(process && process->frame.x > graphs[1]->frame.x + graphs[1]->frame.width && process->key.rfind("expandedTrailing/", 0) == 0, "the list at the right");
}

void standardPageTests()
{
    const Tokens& t = tokens();
    StatsReading full = fullReading();

    // Network: two graphs on their own scales, today's totals.
    Content net = networkPage(viewOf(full, StatsPage::Network));
    expect(net.card == CardLayout::Centred && !net.wideCard && net.backAction == stats_action::kBack && net.expandedCenter.size() == 4 && net.pageAtTop,
           "a page at the card's usual width");
    const Row& header = net.expandedCenter[0];
    expect(std::get<Icon>(header.trailing[0]).glyph == Glyph::Wifi && std::get<Text>(header.trailing[1]).value == "Orchard-5G", "the Wi-Fi's name");
    const Graph& down = std::get<Graph>(net.expandedCenter[1].items[0]);
    const Graph& up = std::get<Graph>(net.expandedCenter[1].items[1]);
    expect(down.tile && down.caption == "↓ Download" && down.value == "717 KB/s" && !down.valueLarge && up.caption == "↑ Upload" && up.value == "307 KB/s" &&
           net.expandedCenter[1].even && !down.alert && down.actionId.empty(), "download and upload, each with its speed");
    expect(std::get<Text>(net.expandedCenter[2].items[0]).value == "4.2 GB" && std::get<Text>(net.expandedCenter[2].items[1]).value == "310 MB" &&
           std::get<Text>(net.expandedCenter[2].items[2]).value == "100 Mbps" && std::get<Text>(net.expandedCenter[3].items[2]).value == "Link speed" &&
           std::get<Text>(net.expandedCenter[2].items[0]).style == TextStyle::Title, "today's totals and the link's speed, each over its name");
    StatsReading wired = full;
    wired.wired = true, wired.networkName.clear(), wired.networkDown = 12.4f * 1000 * 1000, wired.networkDownToday.reset(), wired.linkSpeed = 100'000'000;
    Content cable = networkPage(viewOf(wired, StatsPage::Network));
    expect(std::get<Icon>(cable.expandedCenter[0].trailing[0]).glyph == Glyph::Ethernet && std::get<Text>(cable.expandedCenter[0].trailing[1]).value == "Ethernet" &&
           std::get<Graph>(cable.expandedCenter[1].items[0]).alert && !std::get<Graph>(cable.expandedCenter[1].items[1]).alert &&
           cable.expandedCenter[2].items.size() == 2, "a cable; a direction at the link's limit marked; only the facts there are");
    Content offline = networkPage({});
    expect(offline.expandedCenter.size() == 2 && offline.expandedCenter[0].trailing.empty() &&
           std::get<Graph>(offline.expandedCenter[1].items[0]).value == kNoReading, "nothing read: the graphs, empty");

    // Bluetooth: a row per device, the emptiest first, in a list.
    Content bt = bluetoothPage(viewOf(full, StatsPage::Bluetooth));
    expect(bt.pageAtTop && bt.expandedCenter.size() == 4 && std::get<Text>(bt.expandedCenter[0].trailing[0]).value == "3 connected" &&
           bt.expandedCenter[0].list.empty() && bt.expandedCenter[1].list == "devices" && bt.expandedCenter[1].visibleRows < 0 && bt.expandedCenter[3].list == "devices",
           "a header over a list that fills the card");
    const Row& first = bt.expandedCenter[1];
    expect(std::get<Icon>(first.leading[0]).glyph == Glyph::Keyboard && std::get<Text>(first.items[0]).value == "Keychron K3" &&
           std::get<Text>(first.items[0]).detail == "Keyboard" && std::get<Battery>(first.trailing[0]) == Battery{ .level = 0.45f } &&
           std::get<Text>(first.trailing[1]).value == "45%", "its glyph, name and kind, battery and percent");
    StatsReading buds = full;
    buds.bluetooth.push_back({ "Galaxy Buds2 Pro", std::nullopt, DeviceKind::Earbuds, { { "Left", 82 }, { "Right", 78 }, { "Case", 12 } } });
    Content withParts = bluetoothPage(viewOf(buds, StatsPage::Bluetooth));
    const Row& budsRow = withParts.expandedCenter[1];
    const Row& partRow = withParts.expandedCenter[2];
    expect(withParts.expandedCenter.size() == 6 && std::get<Text>(budsRow.items[0]).value == "Galaxy Buds2 Pro" && std::get<Text>(budsRow.items[0]).detail == "Low battery" &&
           std::get<Battery>(budsRow.trailing[0]).alert && std::get<Text>(budsRow.trailing[1]).value == "12%", "earbuds as low as their emptiest part, and saying so");
    expect(partRow.even && partRow.items.size() == 3 && partRow.list == "devices" && std::get<Battery>(partRow.items[0]) == Battery{ 0.82f, false, "Left", "82%" } &&
           std::get<Battery>(partRow.items[2]) == Battery{ 0.12f, true, "Case", "12%" }, "their parts under them, each with its battery");
    Content none = bluetoothPage({});
    expect(none.expandedCenter.size() == 4 && std::get<Icon>(none.expandedCenter[1].items[0]) == Icon{ Glyph::Bluetooth, true } &&
           std::get<Text>(none.expandedCenter[2].items[0]).value == "No devices connected" && none.expandedCenter[1].list.empty() &&
           none.expandedCenter[0].trailing.empty(), "no devices: media's empty card, under the header");
    StatsReading crowd = buds;
    for (int i = 0; i < 12; ++i) crowd.bluetooth.push_back({ "Device " + std::to_string(i), 50.0f + static_cast<float>(i) });

    for (const StatsReading& r : { full, wired, buds, crowd, StatsReading{} })
        for (StatsPage page : { StatsPage::Network, StatsPage::Bluetooth }) {
            Scene scene = shown(viewOf(r, page, 50));
            std::string what = openAction(page);
            expect(scene.size.x == t.expandedSize.x && scene.size.y == t.expandedSize.y, what + ": the card's usual size");
            expectFits(scene, what);
            auto back = hitTest(scene, { t.expandedPadding + 4, t.expandedPadding + 4 });
            expect(back && back->action.actionId == stats_action::kBack, what + ": the back arrow at its top left");
        }
    // More devices than fit: the list scrolls in the card, under the header.
    Scene scene = shown(viewOf(crowd, StatsPage::Bluetooth));
    const Placed* row = textNamed(scene, "Galaxy Buds2 Pro");
    const Placed* header2 = textNamed(scene, "Bluetooth");
    expect(row && row->list && row->list->max > 0 && header2 && !header2->list && header2->frame.y < row->list->viewport.y &&
           std::abs(row->list->viewport.y + row->list->viewport.height - (scene.size.y - t.expandedPadding)) < 0.51f, "a list that scrolls, down to the card's padding");
    Scene few = shown(viewOf(full, StatsPage::Bluetooth));
    expect(textNamed(few, "Keychron K3")->list->max == 0, "a few devices: nothing to scroll");
}

} // namespace

void runStatisticsPageTests()
{
    widePageTests();
    standardPageTests();
}

} // namespace cup::test
