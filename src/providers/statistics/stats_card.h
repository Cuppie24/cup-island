#pragma once
// The statistics card, pure (decision 172): an overview of tiles, each a
// button into a page of its own within the card, with a back arrow. What's
// shown is a StatsView: the last reading, the histories and the page.
// stats_overview.cpp builds the overview, stats_pages.cpp the standard pages
// (network, Bluetooth), stats_pages_wide.cpp the wide ones (CPU, GPU,
// memory, disk). One small function per tile or row: easy to rearrange.

#include "core/island.h"
#include "providers/statistics/stats_format.h"
#include "providers/statistics/stats_history.h"
#include "providers/statistics/stats_reading.h"

#include <optional>
#include <string>

namespace cup {

inline constexpr const char* kStatisticsPanelId = "statistics.system"; // its panel: a page, never an activity

enum class StatsPage { Overview, Cpu, Gpu, Memory, Network, Disk, Bluetooth };

namespace stats_action {
inline constexpr const char* kOpenCpu = "open.cpu"; // a tile of the overview: its page
inline constexpr const char* kOpenGpu = "open.gpu";
inline constexpr const char* kOpenMemory = "open.memory";
inline constexpr const char* kOpenNetwork = "open.network";
inline constexpr const char* kOpenDisk = "open.disk";
inline constexpr const char* kOpenBluetooth = "open.bluetooth";
inline constexpr const char* kBack = "back"; // a page's arrow, and Esc on it (Content::backAction)
} // namespace stats_action

struct StatsView {
    StatsReading reading;
    StatsHistory history;
    StatsPage page = StatsPage::Overview;
    bool operator==(const StatsView&) const = default;
};

// The action of the overview's tile that opens `page` ("" for the overview), and back again.
const char* openAction(StatsPage page);
std::optional<StatsPage> pageOpenedBy(const std::string& actionId);
// Whether the overview has a tile for it, so it can be opened: no GPU
// reading, no GPU page; no drives, no disk page. The overview: always.
bool hasPage(const StatsReading& reading, StatsPage page);

// The overview: CPU, GPU and memory as history tiles; under them the
// network, the system drive and Bluetooth batteries. One height with every
// card (expandedSize.y).
Content overviewContent(const StatsView& view);
// The overview's rows alone: every page keeps the height they give the card (Content::sizedLike).
Slot overviewRows(const StatsView& view);

// A page's header: the back arrow at the left, its title centred, `trailing` at the right.
Row pageHeader(const std::string& title, std::vector<Item> trailing = {});
// A page's Content around its rows: the overview's height, Esc goes back.
Content pageContent(const StatsView& view, const std::string& label);
// A row of a page's side list: a name, a short bar at `level` (0..1) and its value, the values in one column.
Row listRow(const std::string& name, float level, const std::string& value, const std::string& widest);
// A history as a graph, its newest sample at the right edge.
Graph historyGraph(std::vector<float> samples, GraphSize size);

Content cpuPage(const StatsView& view);
Content gpuPage(const StatsView& view);
Content memoryPage(const StatsView& view);
Content networkPage(const StatsView& view);
Content diskPage(const StatsView& view);
Content bluetoothPage(const StatsView& view);

// What the card shows: the view's page, or the overview if that page has nothing to show any more.
Content statisticsContent(const StatsView& view);
// Its page in the page row: the card under a gauge (decision 165).
Panel statisticsPanel(const StatsView& view);

// A device kind's glyph; the plain Bluetooth one for any other.
Glyph glyphOf(DeviceKind kind);
// "Disk C:" from "Windows (C:)"; a label with no letter as it is.
std::string driveCaption(const std::string& label);

} // namespace cup
