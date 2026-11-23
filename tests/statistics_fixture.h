#pragma once
// A reading with everything on it, as the mockup's laptop, and the view the
// provider would hold after a few of them; a view laid out as its card, and
// the check that everything on it fits. For the statistics tests.

#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
#include "providers/statistics/stats_card.h"

#include <string>
#include <vector>

namespace cup::test {

inline constexpr std::uint64_t kMb = 1024ull * 1024, kGb = 1024 * kMb;

// A laptop with everything readable, as the mockup's.
inline StatsReading fullReading()
{
    StatsReading r;
    r.cpuLoad = 58, r.cpuTemperature = 73, r.cpuName = "AMD Ryzen 7 7840U";
    r.coreLoads = { 40, 62, 95, 12, 58, 71, 33, 60 };
    r.coreTemperatures = { 70, 72, 88, 66, 73, 75, 69, 74 };
    r.cpuProcesses = { { "Spotify", 2.3 }, { "Chrome", 11.6 }, { "Explorer", 1.2 }, { "Visual Studio Code", 5.6 }, { "Windows Defender", 4.1 }, { "Idle", 0.1 } };
    r.gpuLoad = 29, r.gpuTemperature = 50, r.gpuName = "GeForce RTX 4060";
    r.gpuMemoryUsed = kGb * 35 / 10, r.gpuMemoryTotal = 8 * kGb;
    r.gpuEngines = { { "3D", 47 }, { "Video decode", 0.4 }, { "Video encode", 1 }, { "Copy", 0 } };
    r.gpuProcesses = { { "Chrome", 9.6 }, { "Desktop Window Manager", 5.0 }, { "Spotify", 1.6 }, { "Other", 0.2 } };
    r.memoryUsed = kGb * 186 / 10, r.memoryTotal = 32 * kGb, r.memoryCached = kGb * 61 / 10;
    r.memoryApps = { { "Chrome", 4.6 * kGb }, { "Docker Desktop", 3.8 * kGb }, { "Slack", 1.2 * kGb }, { "Spotify", 700.0 * kMb } };
    r.networkDown = 0.7f * kMb, r.networkUp = 0.3f * kMb;
    r.linkSpeed = 100'000'000, r.networkName = "Orchard-5G";
    r.drives = { { "Windows (C:)", 364 * kGb, 512 * kGb }, { "Data (D:)", 612 * kGb, 1000 * kGb }, { "Backup (E:)", 1310 * kGb, 2000 * kGb } };
    r.diskRead = 8.0f * kMb, r.diskWrite = 3.0f * kMb;
    r.bluetooth = { { "MX Master 3S", 60.0f, DeviceKind::Mouse }, { "WH-1000XM5", 82.0f, DeviceKind::Headphones }, { "Keychron K3", 45.0f, DeviceKind::Keyboard } };
    return r;
}

// What the provider would show after `readings` of `reading`.
inline StatsView viewOf(const StatsReading& reading, StatsPage page = StatsPage::Overview, int readings = 3)
{
    StatsView view{ .reading = reading, .page = page };
    for (int i = 0; i < readings; ++i) record(view.history, reading);
    return view;
}

inline Scene shown(const StatsView& view)
{
    IslandState state;
    state = reduce(state, PanelPublished{ statisticsPanel(view) }, Ms{ 0 });
    state = reduce(state, PillClicked{}, Ms{ 0 }); // the idle pill opens the first page (decision 170)
    return layout(state, Ms{ 0 });
}

template <typename T> inline std::vector<const Placed*> all(const Scene& scene)
{
    std::vector<const Placed*> found;
    for (const Placed& p : scene.elements)
        if (std::holds_alternative<T>(p.item)) found.push_back(&p);
    return found;
}

inline const Placed* textNamed(const Scene& scene, const std::string& value)
{
    for (const Placed& p : scene.elements)
        if (auto* text = std::get_if<Text>(&p.item); text && text->value == value) return &p;
    return nullptr;
}

// Everything the card's content placed is inside its padding, and nothing a click reaches is under another.
inline void expectFits(const Scene& scene, const std::string& what)
{
    const Tokens& t = tokens();
    std::size_t placed = 0;
    for (const Placed& p : scene.elements) {
        if (p.key.rfind("expanded", 0) != 0) continue;
        ++placed;
        Rect f = p.list ? p.list->viewport : p.frame; // a list's rows scroll inside its window
        expect(f.x >= t.expandedPadding - 0.01f && f.x + f.width <= scene.size.x - t.expandedPadding + 0.01f && f.y >= t.expandedPadding - 0.01f &&
               f.y + f.height <= scene.size.y - t.expandedPadding + 0.01f && p.frame.width >= 0 && p.frame.height >= 0,
               what + ": fits its card: " + p.key);
    }
    expect(placed > 0, what + ": something is on it");
    // Rows of one slot never overlap: each starts where the one above ended, or lower.
    for (const Placed& a : scene.elements)
        for (const Placed& b : scene.elements) {
            if (a.list || b.list || a.key.rfind("expanded", 0) != 0 || b.key.rfind("expanded", 0) != 0) continue;
            std::string slotA = a.key.substr(0, a.key.find('/')), slotB = b.key.substr(0, b.key.find('/'));
            std::string rowA = a.key.substr(0, a.key.rfind('/')), rowB = b.key.substr(0, b.key.rfind('/'));
            if (slotA != slotB || rowA == rowB || a.frame.y > b.frame.y || a.frame.height == 0) continue;
            expect(a.frame.y + a.frame.height <= b.frame.y + 0.01f || a.frame.y == b.frame.y, what + ": rows don't overlap: " + a.key + " / " + b.key);
        }
}

} // namespace cup::test
