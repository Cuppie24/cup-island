#include "core/home.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

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

// A tile's place in the grid, in columns and rows.
struct Cell {
    Entry entry;
    std::size_t column, row, span;
};

// Row by row. A wide tile that doesn't fit the rest of a row starts the
// next one, so tiles never change size to fit.
std::vector<Cell> cellsFor(const std::vector<Entry>& entries, std::size_t columns)
{
    std::vector<Cell> cells;
    std::size_t column = 0, row = 0;
    for (const Entry& entry : entries) {
        std::size_t span = entry.wide && columns > 1 ? 2 : 1;
        if (column + span > columns) {
            column = 0;
            ++row;
        }
        cells.push_back({ entry, column, row, span });
        column += span;
    }
    return cells;
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

    std::size_t columns = std::max<std::size_t>(t.homeColumns, 1);
    std::vector<Cell> cells = cellsFor(entries, columns);
    if (cells.empty()) return {};

    float p = t.expandedPadding, gap = t.tileGap, h = t.tileHeight, n = static_cast<float>(columns);
    float columnWidth = (cardSize.x - 2 * p - (n - 1) * gap) / n;
    float areaTop = p, areaHeight = cardSize.y - 2 * p;

    // As many rows as fit, centred vertically in the card (which keeps Expanded's size).
    auto fit = static_cast<std::size_t>(std::max(0.0f, std::floor((areaHeight + gap) / (h + gap))));
    std::size_t rows = std::min(fit, cells.back().row + 1);
    float gridHeight = static_cast<float>(rows) * (h + gap) - gap;
    float y0 = areaTop + (areaHeight - gridHeight) / 2;

    std::vector<Tile> tiles;
    for (const Cell& cell : cells) {
        if (cell.row >= rows) break;
        const Entry& e = cell.entry;
        float span = static_cast<float>(cell.span);
        Rect frame{ p + static_cast<float>(cell.column) * (columnWidth + gap), y0 + static_cast<float>(cell.row) * (h + gap),
                    span * columnWidth + (span - 1) * gap, h };
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
