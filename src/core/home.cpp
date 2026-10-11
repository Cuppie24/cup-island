#include "core/home.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

// What one tile shows: a panel (live or dormant), or the clock.
struct Entry {
    std::string id;
    std::string providerId;
    const Content* content; // whose `tile` slot to place
    bool wide = false;
    bool opens = false;
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

// The clock: the time large, today's date under it. Built by the host from
// the clock it already keeps (the idle pill's), so no provider ticks it.
Content clockTile(Ms timeOfDay, const WallClock& clock)
{
    Content c{ .label = "Clock" };
    c.tile = { row({ Text{ formatClock(timeOfDay, clock.hour24), TextStyle::Title } }) };
    if (!clock.date.empty()) c.tile.push_back(row({ Text{ clock.date, TextStyle::Caption } }));
    return c;
}

} // namespace

std::vector<Tile> homeTilesFor(const IslandState& s, Vec2 cardSize, float tileRadius, Ms now,
                               std::vector<Placed>& elements)
{
    const Tokens& t = tokens();

    // The clock first, then panels in publish order. A running panel shows its
    // activity's tile, if it gives one (decision 009).
    std::vector<Entry> entries;
    Content clock;
    if (auto time = timeOfDay(s, now)) {
        clock = clockTile(*time, *s.clock);
        entries.push_back({ "", "", &clock });
    }
    for (const Panel& panel : s.panels) {
        const Activity* live = findActivity(s, panel.id);
        const Content& content = live && !live->content.tile.empty() ? live->content : panel.content;
        entries.push_back({ panel.id, panel.providerId, &content, panel.wide, live || hasCard(panel.content) });
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
        tiles.push_back({ frame, e.id, e.opens });

        float inset = t.tilePadding;
        Rect inner{ frame.x + inset, frame.y + inset, std::max(0.0f, frame.width - 2 * inset), std::max(0.0f, frame.height - 2 * inset) };
        placeSlot({ e.id.empty() ? "tile/clock" : "tile/" + e.id, &e.content->tile, inner }, { e.content, e.providerId, e.id },
                  std::max(0.0f, tileRadius - inset), now, elements);
    }
    return tiles;
}

} // namespace cup
