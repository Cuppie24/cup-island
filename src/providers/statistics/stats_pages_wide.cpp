// The wide pages (decision 172): a block of graphs at the left, a list at the
// right (CardLayout::Columns on a wide card). CPU, GPU, memory and disks.

#include "providers/statistics/stats_card.h"

#include "core/tokens.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace cup {
namespace {

Content widePage(const StatsView& v, const std::string& label, Row header)
{
    Content c = pageContent(v, label);
    c.card = CardLayout::Columns;
    c.wideCard = true; // a list beside graphs needs the width (decision 085)
    c.expandedCenter = { std::move(header) };
    return c;
}

Text caption(std::string value)
{
    return Text{ .value = std::move(value), .style = TextStyle::Caption, .brightDigits = true };
}

// The page's big numbers, side by side: what each is, the number, its history.
// One function for the CPU's and the GPU's, so the two pages read as a pair.
struct Reading {
    std::string name;  // "Load"
    std::string value; // "58%"
    Item history;      // its graph, or a line saying why there's none
};
void readingRows(Slot& rows, std::vector<Reading> readings)
{
    Row names{ .loose = true, .even = true }, values = names, graphs = names;
    for (Reading& r : readings) {
        names.items.push_back(caption(std::move(r.name)));
        values.items.push_back(Text{ .value = std::move(r.value), .style = TextStyle::Hero });
        graphs.items.push_back(std::move(r.history));
    }
    rows.insert(rows.end(), { names, values, graphs });
}

Reading loadReading(std::string name, std::optional<float> load, const StatsHistory::Samples& samples)
{
    Graph graph = historyGraph(percentSamples(samples), GraphSize::Strip);
    graph.alert = highLoad(load);
    return { std::move(name), percentText(load), Item{ std::move(graph) } };
}

Reading temperatureReading(std::optional<float> celsius, const StatsHistory::Samples& samples)
{
    Graph graph = historyGraph(temperatureSamples(samples), GraphSize::Strip);
    graph.limit = temperatureLimit(); // where it turns hot
    graph.alert = highTemperature(celsius);
    std::string reads = temperatureText(celsius);
    return { "Temperature", reads.empty() ? kNoReading : reads, Item{ std::move(graph) } }; // read before, not now: its history stays
}

// No temperature (most PCs, without admin rights or a vendor's API): the
// column keeps its place, a dash for the number and why where its graph would be,
// so the page has one shape on every PC.
Reading noTemperature()
{
    return { "Temperature", kNoReading, Text{ .value = "Isn't reported on this PC", .style = TextStyle::Caption } };
}

// The temperature's column: its reading if it was ever read while the card was open, else that there's none.
Reading temperatureColumn(std::optional<float> celsius, const StatsHistory::Samples& samples)
{
    return anyReading(samples) || cleanTemperature(celsius) ? temperatureReading(celsius, samples) : noTemperature();
}

// A list at the right: its heading, then a row each; `relative`: the bars
// against the biggest in the list (shares that are all small still tell apart).
void sideList(Slot& rows, const char* heading, const std::vector<NamedValue>& values, bool relative, const std::string& widest,
              const std::function<std::string(double)>& text)
{
    if (values.empty()) return;
    rows.push_back(row({ Text{ .value = heading, .style = TextStyle::Caption } }));
    double most = relative ? values.front().value : 100; // sorted: the first is the biggest
    for (const NamedValue& v : values)
        rows.push_back(listRow(v.name, most > 0 ? static_cast<float>(v.value / most) : 0.0f, text(v.value), widest));
}

Levels coreLoads(const std::vector<float>& loads)
{
    Levels levels;
    for (std::size_t i = 0; i < loads.size(); ++i) {
        auto load = cleanPercent(loads[i]);
        levels.bars.push_back({ load.value_or(0) / 100, highLoad(load), "Core " + std::to_string(i + 1) + " · " + percentText(load) });
    }
    return levels;
}

// On the temperature graph's scale, so a core's bar and the history's line agree.
Levels coreTemperatures(const std::vector<float>& temperatures)
{
    Levels levels;
    for (std::size_t i = 0; i < temperatures.size(); ++i) {
        auto celsius = cleanTemperature(temperatures[i]);
        std::string reads = celsius ? temperatureText(celsius) : std::string(kNoReading);
        float level = celsius ? temperatureSamples({ celsius }).front() : 0.0f;
        levels.bars.push_back({ level, highTemperature(celsius), "Core " + std::to_string(i + 1) + " · " + reads });
    }
    return levels;
}

// A drive: its name with used of total and the percent, over its level.
void driveRows(Slot& rows, const DriveReading& drive, bool apart)
{
    auto percent = usedPercent(drive.used, drive.total);
    Row name{ .items = { Text{ .value = drive.label.empty() ? "Disk" : drive.label, .style = TextStyle::Body } },
              .trailing = { caption(usedOfText(drive.used, drive.total)),
                            Text{ .value = percentText(percent), .style = TextStyle::Caption, .alignEnd = true, .brightDigits = true, .sizedAs = "100%" } } };
    name.apart = apart;
    rows.push_back(std::move(name));
    rows.push_back(row({ Progress{ .level = percent.value_or(0) / 100, .alert = highLoad(percent) } }));
}

Graph activityTile(const char* name, std::optional<float> speed, const StatsHistory::Samples& samples)
{
    Graph graph = historyGraph(speedSamples(samples), GraphSize::Low);
    graph.tile = true;
    graph.caption = name;
    std::string now = speedText(speed);
    graph.value = now.empty() ? kNoReading : now;
    return graph;
}

} // namespace

Content cpuPage(const StatsView& v)
{
    const StatsReading& r = v.reading;
    Content c = widePage(v, "CPU", pageHeader("CPU", { Text{ .value = r.cpuName, .style = TextStyle::Caption } }));
    std::string cores = r.coreLoads.empty() ? "" : " · " + std::to_string(r.coreLoads.size()) + " cores";
    readingRows(c.expandedLeading, { loadReading("Load" + cores, r.cpuLoad, samplesOf(v.history, Series::CpuLoad)),
                                     temperatureColumn(r.cpuTemperature, samplesOf(v.history, Series::CpuTemperature)) });
    // A bar per core under each history; a column with no cores to show keeps its place empty.
    if (!r.coreLoads.empty() || !r.coreTemperatures.empty()) {
        Row strips{ .loose = true, .even = true };
        strips.items.push_back(r.coreLoads.empty() ? Item{ Text{} } : Item{ coreLoads(r.coreLoads) });
        strips.items.push_back(r.coreTemperatures.empty() ? Item{ Text{} } : Item{ coreTemperatures(r.coreTemperatures) });
        c.expandedLeading.push_back(std::move(strips));
    }
    sideList(c.expandedTrailing, "Top processes", topValues(r.cpuProcesses, tokens().statsListShown), true, "99.9%", shareText);
    return c;
}

Content gpuPage(const StatsView& v)
{
    const StatsReading& r = v.reading;
    Content c = widePage(v, "GPU", pageHeader("GPU", { Text{ .value = r.gpuName, .style = TextStyle::Caption } }));
    readingRows(c.expandedLeading, { loadReading("Load", r.gpuLoad, samplesOf(v.history, Series::GpuLoad)),
                                     temperatureColumn(r.gpuTemperature, samplesOf(v.history, Series::GpuTemperature)) });
    // Dedicated memory under both, where the CPU's page has its cores.
    if (auto memory = usedPercent(r.gpuMemoryUsed, r.gpuMemoryTotal))
        c.expandedLeading.push_back(Row{ .items = { Progress{ .level = *memory / 100, .alert = highLoad(memory) } },
                                         .leading = { Text{ .value = "Memory", .style = TextStyle::Caption } },
                                         .trailing = { caption(usedOfText(r.gpuMemoryUsed, r.gpuMemoryTotal)) } });
    // What it's busy with, then who is using it; each a share of the list's room.
    std::size_t each = std::max<std::size_t>(tokens().statsListShown / 2 + 1, 1);
    sideList(c.expandedTrailing, "Engines", topValues(r.gpuEngines, each), false, "99.9%", shareText);
    sideList(c.expandedTrailing, "Top processes", topValues(r.gpuProcesses, each), true, "99.9%", shareText);
    return c;
}

Content memoryPage(const StatsView& v)
{
    const StatsReading& r = v.reading;
    auto percent = usedPercent(r.memoryUsed, r.memoryTotal);
    Content c = widePage(v, "Memory", pageHeader("Memory", { caption(r.memoryTotal && percent ? sizeText(*r.memoryTotal) : "") }));
    Graph graph = historyGraph(percentSamples(samplesOf(v.history, Series::Memory)), GraphSize::Tall);
    graph.tile = true;
    graph.caption = "In use";
    graph.valueLarge = true;
    graph.alert = highLoad(percent);
    graph.value = percentText(percent);
    c.expandedLeading = { row({ graph }) };
    if (percent) {
        std::uint64_t total = *r.memoryTotal, used = std::min(*r.memoryUsed, total);
        std::get<Graph>(c.expandedLeading[0].items[0]).value = sizeText(used) + " · " + percentText(percent);
        // In use, then cached (given back when asked for: "full" memory that is mostly cache is fine), the rest free.
        Progress parts{ .level = *percent / 100, .alert = highLoad(percent), .thick = true };
        Row legend{ .align = Align::Center, .even = true };
        auto part = [](const char* name, std::uint64_t bytes) {
            return Text{ .value = std::string(name) + " " + sizeText(bytes), .style = TextStyle::Caption, .centred = true, .brightDigits = true };
        };
        legend.items.push_back(part("Used", used));
        if (r.memoryCached) {
            std::uint64_t cached = std::min(*r.memoryCached, total - used);
            parts.second = static_cast<float>(static_cast<double>(cached) / static_cast<double>(total));
            legend.items.push_back(part("Cached", cached));
            legend.items.push_back(part("Free", total - used - cached));
        } else {
            legend.items.push_back(part("Free", total - used));
        }
        c.expandedLeading.push_back(row({ parts }));
        c.expandedLeading.push_back(std::move(legend));
    }
    sideList(c.expandedTrailing, "Top apps", topValues(r.memoryApps, tokens().statsListShown), true, "999 MB",
             [](double bytes) { return sizeText(static_cast<std::uint64_t>(bytes)); });
    return c;
}

Content diskPage(const StatsView& v)
{
    const StatsReading& r = v.reading;
    std::size_t count = r.drives.size(), shown = std::min(count, tokens().statsDrivesShown);
    Content c = widePage(v, "Disk", pageHeader("Disk", { caption(std::to_string(count) + (count == 1 ? " drive" : " drives")) }));
    // A few drives stand apart; many sit closer, so they all fit.
    for (std::size_t i = 0; i < shown; ++i) driveRows(c.expandedLeading, r.drives[i], i > 0 && shown <= 3);
    // How hard the disks are working, when the reader can say.
    bool read = anyReading(samplesOf(v.history, Series::DiskRead)), write = anyReading(samplesOf(v.history, Series::DiskWrite));
    if (read) c.expandedTrailing.push_back(row({ activityTile("Read", r.diskRead, samplesOf(v.history, Series::DiskRead)) }));
    if (write) {
        Row under = row({ activityTile("Write", r.diskWrite, samplesOf(v.history, Series::DiskWrite)) });
        under.tileGap = read;
        c.expandedTrailing.push_back(std::move(under));
    }
    return c;
}

} // namespace cup
