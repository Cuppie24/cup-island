#include "providers/shelf/drop_plan.h"

#include "core/tokens.h"

#include <cstdio>
#include <optional>

namespace cup::shelf {
namespace {

constexpr std::uint64_t kMB = 1024 * 1024;

// Why an item can't be copied.
enum class Cause { None, Folder, TooLarge, StoreFull, NoSpace };

struct Budget {
    std::uint64_t store = 0;                // room left in the shelf's store
    std::optional<std::uint64_t> disk;      // free space on its disk, once known
};

bool canLink(const DragItem& item) { return item.kind == DragKind::File || item.kind == DragKind::Folder; }

// Can it be copied with what's left, and if not, why. A file whose size isn't
// read yet is taken as fitting: the limit then applies during the copy.
Cause copyCause(const DragItem& item, const Limits& limits, const Budget& budget)
{
    if (item.kind == DragKind::Folder) return Cause::Folder;
    std::uint64_t bytes = item.bytes.value_or(0);
    if (bytes > limits.copyMaxBytes) return Cause::TooLarge;
    if (bytes > budget.store) return Cause::StoreFull;
    if (budget.disk && bytes > *budget.disk) return Cause::NoSpace;
    return Cause::None;
}

std::string causeText(Cause cause, std::size_t count)
{
    switch (cause) {
    case Cause::Folder: return count == 1 ? "a folder" : "folders";
    case Cause::TooLarge: return "too large";
    case Cause::StoreFull: return "over the shelf's storage";
    case Cause::NoSpace: return "no room on the disk";
    case Cause::None: break;
    }
    return "";
}

// The greyed Copy zone's line: the first cause, with the size where it helps.
std::string copyReason(Cause cause, const DragItem& item, const Limits& limits, std::size_t items)
{
    switch (cause) {
    case Cause::Folder: return "Folders can only be linked";
    case Cause::TooLarge:
        if (items == 1 && item.bytes) return "Too large to copy (" + formatBytes(*item.bytes) + ", limit " + formatBytes(limits.copyMaxBytes) + ")";
        return "Too large to copy (limit " + formatBytes(limits.copyMaxBytes) + ")";
    case Cause::StoreFull: return "Shelf storage full";
    case Cause::NoSpace: return "Not enough free space";
    case Cause::None: break;
    }
    return "";
}

std::string count(const char* verb, std::size_t done, std::size_t total)
{
    return total == 1 ? std::string(verb) : std::string(verb) + " " + std::to_string(done);
}

ZonePlan planLink(const DragSummary& drag)
{
    ZonePlan zone;
    std::size_t linked = 0, copied = 0, skipped = 0;
    for (const DragItem& item : drag.items) {
        Handling h = canLink(item) ? Handling::Link : item.kind == DragKind::Text || item.kind == DragKind::Url ? Handling::Copy : Handling::Skip;
        zone.items.push_back(h);
        linked += h == Handling::Link;
        copied += h == Handling::Copy;
        skipped += h == Handling::Skip;
    }
    zone.enabled = linked > 0;
    if (!zone.enabled) {
        zone.label = "Link";
        zone.reason = "Nothing to link: text and links are saved as files";
        return zone;
    }
    std::size_t total = drag.total > drag.items.size() ? drag.total : drag.items.size();
    zone.label = count("Link", total > drag.items.size() ? total : linked, total);
    if (copied) zone.label += " · " + std::to_string(copied) + (copied == 1 ? " has" : " have") + " no file, will be copied";
    if (skipped) zone.label += " · " + std::to_string(skipped) + " skipped";
    zone.reason = "Points to the original";
    return zone;
}

ZonePlan planCopy(const DragSummary& drag, const Limits& limits)
{
    ZonePlan zone;
    if (drag.total > limits.maxChecked) { // only counted: linking is all that's safe
        zone.items.assign(drag.items.size(), Handling::Link);
        zone.label = "Copy";
        zone.reason = "Large drop: link only";
        return zone;
    }
    Budget budget{ limits.storeMaxBytes > limits.storeUsed ? limits.storeMaxBytes - limits.storeUsed : 0, drag.freeBytes };
    std::size_t copied = 0, linked = 0, skipped = 0;
    Cause firstCause = Cause::None, linkCause = Cause::None, skipCause = Cause::None;
    bool mixedLink = false, mixedSkip = false;
    const DragItem* firstRefused = nullptr;
    bool unknownSizes = false;
    for (const DragItem& item : drag.items) {
        Cause cause = copyCause(item, limits, budget);
        if (cause == Cause::None) {
            zone.items.push_back(Handling::Copy);
            ++copied;
            std::uint64_t bytes = item.bytes.value_or(0);
            budget.store -= bytes; // fills in drop order
            if (budget.disk) *budget.disk -= bytes;
            unknownSizes = unknownSizes || (item.kind == DragKind::File && !item.bytes);
            continue;
        }
        if (firstCause == Cause::None) { firstCause = cause; firstRefused = &item; }
        Handling h = canLink(item) ? Handling::Link : Handling::Skip;
        zone.items.push_back(h);
        Cause& seen = h == Handling::Link ? linkCause : skipCause;
        bool& mixed = h == Handling::Link ? mixedLink : mixedSkip;
        if (seen != Cause::None && seen != cause) mixed = true;
        seen = cause;
        (h == Handling::Link ? linked : skipped)++;
    }
    zone.enabled = copied > 0;
    if (!zone.enabled) {
        zone.label = "Copy";
        zone.reason = firstRefused ? copyReason(firstCause, *firstRefused, limits, drag.items.size()) : "Nothing to copy";
        return zone;
    }
    zone.label = count("Copy", copied, drag.items.size());
    if (linked) zone.label += " · " + std::to_string(linked) + (mixedLink ? "" : " " + causeText(linkCause, linked)) + ", will be linked";
    if (skipped) zone.label += " · " + std::to_string(skipped) + " skipped" + (mixedSkip ? "" : " (" + causeText(skipCause, skipped) + ")");
    if (!drag.sizesKnown && unknownSizes) zone.reason = "Checking sizes…";
    else if (firstCause == Cause::TooLarge && firstRefused->bytes) zone.reason = formatBytes(*firstRefused->bytes) + ", limit " + formatBytes(limits.copyMaxBytes);
    else zone.reason = "A copy, kept on the shelf";
    return zone;
}

} // namespace

Limits limitsNow(std::uint64_t storeUsed)
{
    const Tokens& t = tokens();
    return { t.shelfCopyMaxMB * kMB, t.shelfStoreMaxMB * kMB, storeUsed, t.shelfMaxCheckedItems };
}

DropPlan planDrop(const DragSummary& drag, const Limits& limits)
{
    return { planLink(drag), planCopy(drag, limits) };
}

std::string formatBytes(std::uint64_t bytes)
{
    const char* units[] = { "bytes", "KB", "MB", "GB", "TB" };
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024 && unit < 4) {
        value /= 1024;
        ++unit;
    }
    char text[32];
    if (unit == 0) std::snprintf(text, sizeof text, "%llu bytes", static_cast<unsigned long long>(bytes));
    else if (value < 10 && value != static_cast<double>(static_cast<int>(value)))
        std::snprintf(text, sizeof text, "%.1f %s", value, units[unit]);
    else std::snprintf(text, sizeof text, "%.0f %s", value, units[unit]);
    std::string out = text;
    if (out.find(".0 ") != std::string::npos) out.erase(out.find(".0 "), 2); // "1.0 GB" reads as "1 GB"
    return out;
}

} // namespace cup::shelf
