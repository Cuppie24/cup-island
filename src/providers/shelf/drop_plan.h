#pragma once
// What the Shelf's two zones would do with a drag (decision 061): Link keeps
// a reference to each original, Copy keeps a copy on the shelf. Each zone
// does all it can and says how ("Copy 2 · 1 too large, will be linked"); it
// is greyed only when it would do nothing, and says why. Pure: the zones'
// labels while the drag is over the island, and the plan for each item once
// it's dropped, come from the same rules.

#include "core/drop.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cup::shelf {

enum class Handling { Copy, Link, Skip };

struct Limits {
    std::uint64_t copyMaxBytes = 0;  // one file, at most
    std::uint64_t storeMaxBytes = 0; // every copy on the shelf together
    std::uint64_t storeUsed = 0;     // ...already taken
    std::size_t maxChecked = 0;      // a drag with more than this is only linked
};

// From tokens (shelfCopyMaxMB, shelfStoreMaxMB, shelfMaxCheckedItems).
Limits limitsNow(std::uint64_t storeUsed);

struct ZonePlan {
    bool enabled = false;
    std::string label;             // "Copy 2 · 1 too large, will be linked"
    std::string reason;            // the line under it: why it's greyed, or what it does
    std::vector<Handling> items;   // what happens to each of the summary's items
    bool operator==(const ZonePlan&) const = default;
};

struct DropPlan {
    ZonePlan link, copy;
    bool operator==(const DropPlan&) const = default;
};

DropPlan planDrop(const DragSummary& drag, const Limits& limits);

// "4.2 GB", "350 MB", "12 KB": one decimal below 10, none above.
std::string formatBytes(std::uint64_t bytes);

} // namespace cup::shelf
