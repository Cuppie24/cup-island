#include "core/home.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

std::size_t spanOf(bool wide, std::size_t columns) { return wide && columns > 1 ? 2 : 1; }

// What one tile shows: a panel, live or dormant.
struct Entry {
    std::string id;
    std::string providerId;
    const Content* content = nullptr; // whose `tile` slots to place
    bool wide = false;
    bool opens = false;
    std::string tapAction;
};

// Rows of widgets, in publish order: each takes one column's worth, or two
// (Panel::wide). One that doesn't fit what's left of a row starts the next.
// Every widget gets a place: none is left out (decision 019).
std::vector<std::vector<Entry>> rowsFor(const std::vector<Entry>& entries, std::size_t columns)
{
    std::vector<std::vector<Entry>> rows;
    std::size_t used = columns;
    for (const Entry& entry : entries) {
        std::size_t span = spanOf(entry.wide, columns);
        if (used + span > columns) {
            rows.emplace_back();
            used = 0;
        }
        rows.back().push_back(entry);
        used += span;
    }
    return rows;
}

// A widget shows information only (decision 019): its buttons and text
// fields are left out, so a click anywhere on it means "open the card".
Slot informationOnly(const Slot& slot)
{
    auto interactive = [](const Item& item) {
        return std::holds_alternative<Button>(item) || std::holds_alternative<TextInput>(item) || std::holds_alternative<Switch>(item);
    };
    Slot out = slot;
    for (Row& r : out)
        for (auto* items : { &r.items, &r.leading, &r.trailing }) std::erase_if(*items, interactive);
    std::erase_if(out, [](const Row& r) { return r.items.empty() && r.leading.empty() && r.trailing.empty(); });
    return out;
}

std::size_t columnCount() { return std::max<std::size_t>(tokens().homeColumns, 1); }

// A widget's content inside its frame: the leading square (art, an icon, a
// ring) as tall as the tile's inside, the rows beside it.
void placeTile(const Entry& e, Rect frame, float tileRadius, Ms now, std::vector<Placed>& elements)
{
    const Tokens& t = tokens();
    float inset = t.tilePadding, radius = std::max(0.0f, tileRadius - inset);
    Rect inner{ frame.x + inset, frame.y + inset, std::max(0.0f, frame.width - 2 * inset), std::max(0.0f, frame.height - 2 * inset) };
    Source source{ e.content, e.providerId, e.id };
    std::string key = "tile/" + e.id;
    if (Slot lead = informationOnly(e.content->tileLeading); !lead.empty()) {
        Rect square{ inner.x, inner.y, inner.height, inner.height };
        placeSlot({ key + "/lead", &lead, square, /* square */ true }, source, radius, now, elements);
        float shift = std::min(inner.width, inner.height + t.slotGap);
        inner = { inner.x + shift, inner.y, inner.width - shift, inner.height };
    }
    Slot rows = informationOnly(e.content->tile);
    placeSlot({ key, &rows, inner }, source, radius, now, elements);
}

} // namespace

std::vector<Tile> homeTilesFor(const IslandState& s, Vec2 cardSize, float tileRadius, Ms now,
                               std::vector<Placed>& elements)
{
    const Tokens& t = tokens();

    // Panels in publish order. A running panel shows its activity's tile, if
    // it gives one (decision 009).
    std::vector<Entry> entries;
    for (const Panel& panel : s.panels) {
        const Activity* live = findActivity(s, panel.id);
        bool liveTile = live && (!live->content.tile.empty() || !live->content.tileLeading.empty());
        const Content& content = liveTile ? live->content : panel.content;
        bool opens = panel.tapAction.empty() && (live || hasCard(panel.content)); // a tap action wins over a card
        entries.push_back({ panel.id, panel.providerId, &content, panel.wide, opens, panel.tapAction });
    }

    std::size_t columns = columnCount();
    std::vector<std::vector<Entry>> rows = rowsFor(entries, columns);
    if (rows.empty()) return {};

    // Every row reaches across the card's full width (decision 019): its
    // widgets share what's left after the gaps by span, so a row that isn't
    // full stretches rather than leaving a hole. The grid is centred
    // vertically; the card grows to fit it (homeCardHeight).
    float p = t.expandedPadding, gap = t.tileGap, h = t.tileHeight;
    float gridHeight = static_cast<float>(rows.size()) * (h + gap) - gap;
    float y = p + std::max(0.0f, cardSize.y - 2 * p - gridHeight) / 2;

    std::vector<Tile> tiles;
    for (const std::vector<Entry>& r : rows) {
        float spans = 0;
        for (const Entry& e : r) spans += static_cast<float>(spanOf(e.wide, columns));
        float unit = (cardSize.x - 2 * p - static_cast<float>(r.size() - 1) * gap) / spans;
        float x = p;
        for (const Entry& e : r) {
            Rect frame{ x, y, static_cast<float>(spanOf(e.wide, columns)) * unit, h };
            x += frame.width + gap;
            tiles.push_back({ frame, e.id, e.opens, e.providerId, e.tapAction, e.content->badge });
            placeTile(e, frame, tileRadius, now, elements);
        }
        y += h + gap;
    }
    return tiles;
}

float homeCardHeight(const IslandState& s, float maxHeight)
{
    const Tokens& t = tokens();
    std::vector<Entry> entries;
    for (const Panel& panel : s.panels) entries.push_back({ .wide = panel.wide });
    std::size_t rows = rowsFor(entries, columnCount()).size();
    float needed = static_cast<float>(rows) * (t.tileHeight + t.tileGap) - t.tileGap + 2 * t.expandedPadding;
    return std::clamp(needed, t.expandedSize.y, std::max(t.expandedSize.y, maxHeight));
}

} // namespace cup
