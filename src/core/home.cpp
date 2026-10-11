#include "core/home.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <utility>

namespace cup {
namespace {

// What one tile shows: a panel, live or dormant.
struct Entry {
    std::string id;
    std::string providerId;
    const Content* content; // whose `tile` slot to place
    bool wide = false;
    bool opens = false;
    std::string tapAction;
};

// One row, in publish order: each widget takes one column's worth, or two
// (Panel::wide); one that doesn't fit what's left of `columns` is left out.
std::vector<std::pair<Entry, std::size_t>> rowFor(const std::vector<Entry>& entries, std::size_t columns)
{
    std::vector<std::pair<Entry, std::size_t>> shown;
    std::size_t used = 0;
    for (const Entry& entry : entries) {
        std::size_t span = entry.wide && columns > 1 ? 2 : 1;
        if (used + span > columns) continue;
        shown.push_back({ entry, span });
        used += span;
    }
    return shown;
}

// A widget shows information only (decision 019): its buttons and text
// fields are left out, so a click anywhere on it means "open the card".
Slot informationOnly(const Slot& slot)
{
    auto interactive = [](const Item& item) { return std::holds_alternative<Button>(item) || std::holds_alternative<TextInput>(item); };
    Slot out = slot;
    for (Row& r : out)
        for (auto* items : { &r.items, &r.leading, &r.trailing }) std::erase_if(*items, interactive);
    std::erase_if(out, [](const Row& r) { return r.items.empty() && r.leading.empty() && r.trailing.empty(); });
    return out;
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

    std::vector<std::pair<Entry, std::size_t>> shown = rowFor(entries, std::max<std::size_t>(t.homeColumns, 1));
    if (shown.empty()) return {};

    // One row across the card's full width (decision 019), centred vertically
    // in it (the card keeps Expanded's size). Widths share what's left after
    // the gaps by span, so the row always reaches both paddings.
    float p = t.expandedPadding, gap = t.tileGap, h = t.tileHeight;
    float spans = 0;
    for (const auto& [entry, span] : shown) spans += static_cast<float>(span);
    float unit = (cardSize.x - 2 * p - static_cast<float>(shown.size() - 1) * gap) / spans;
    float y = p + (cardSize.y - 2 * p - h) / 2;

    std::vector<Tile> tiles;
    float x = p;
    for (const auto& [e, span] : shown) {
        Rect frame{ x, y, static_cast<float>(span) * unit, h };
        x += frame.width + gap;
        tiles.push_back({ frame, e.id, e.opens, e.providerId, e.tapAction });

        // The leading square (art, an icon, a ring) as tall as the tile's inside, the rows beside it.
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
    return tiles;
}

} // namespace cup
