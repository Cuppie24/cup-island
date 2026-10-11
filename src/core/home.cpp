#include "core/home.h"

#include "core/slot_layout.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

// A tile's place in the grid, in columns and rows.
struct Cell {
    const Panel* panel;
    std::size_t column, row, span;
};

// Publish order, row by row. A wide tile that doesn't fit the rest of a row
// starts the next one, so tiles never change size to fit.
std::vector<Cell> cellsFor(const IslandState& s, std::size_t columns)
{
    std::vector<Cell> cells;
    std::size_t column = 0, row = 0;
    for (const Panel& panel : s.panels) {
        std::size_t span = panel.wide && columns > 1 ? 2 : 1;
        if (column + span > columns) {
            column = 0;
            ++row;
        }
        cells.push_back({ &panel, column, row, span });
        column += span;
    }
    return cells;
}

} // namespace

std::vector<Tile> homeTilesFor(const IslandState& s, Vec2 cardSize, float top, float tileRadius, Ms now,
                               std::vector<Placed>& elements)
{
    const Tokens& t = tokens();
    std::size_t columns = std::max<std::size_t>(t.homeColumns, 1);
    std::vector<Cell> cells = cellsFor(s, columns);
    if (cells.empty()) return {};

    float p = t.expandedPadding, gap = t.tileGap, h = t.tileHeight, n = static_cast<float>(columns);
    float columnWidth = (cardSize.x - 2 * p - (n - 1) * gap) / n;
    float areaTop = p + top, areaHeight = cardSize.y - p - areaTop;

    // As many rows as fit, centred vertically in the card (which keeps Expanded's size).
    auto fit = static_cast<std::size_t>(std::max(0.0f, std::floor((areaHeight + gap) / (h + gap))));
    std::size_t rows = std::min(fit, cells.back().row + 1);
    float gridHeight = static_cast<float>(rows) * (h + gap) - gap;
    float y0 = areaTop + (areaHeight - gridHeight) / 2;

    std::vector<Tile> tiles;
    for (const Cell& cell : cells) {
        if (cell.row >= rows) break;
        const Panel& panel = *cell.panel;
        float span = static_cast<float>(cell.span);
        Rect frame{ p + static_cast<float>(cell.column) * (columnWidth + gap), y0 + static_cast<float>(cell.row) * (h + gap),
                    span * columnWidth + (span - 1) * gap, h };
        tiles.push_back({ frame, panel.id });

        // Live: the activity's tile, if it gives one (decision 009).
        const Activity* live = findActivity(s, panel.id);
        const Content& content = live && !live->content.tile.empty() ? live->content : panel.content;
        float inset = t.tilePadding;
        Rect inner{ frame.x + inset, frame.y + inset, std::max(0.0f, frame.width - 2 * inset), std::max(0.0f, frame.height - 2 * inset) };
        placeSlot({ "tile/" + panel.id, &content.tile, inner }, { &content, panel.providerId, panel.id },
                  std::max(0.0f, tileRadius - inset), now, elements);
    }
    return tiles;
}

} // namespace cup
