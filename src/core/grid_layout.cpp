#include "core/grid_layout.h"

#include "core/item_metrics.h"
#include "core/tokens.h"

#include <algorithm>
#include <cmath>

namespace cup {
namespace {

Rect clippedTo(Rect r, Rect clip)
{
    float left = std::max(r.x, clip.x), top = std::max(r.y, clip.y);
    float right = std::min(r.x + r.width, clip.x + clip.width), bottom = std::min(r.y + r.height, clip.y + clip.height);
    return { left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top) };
}

} // namespace

std::size_t gridColumns(float width)
{
    const Tokens& t = tokens();
    float fits = std::floor((width + t.gridGap) / (t.gridCellMinWidth + t.gridGap));
    return static_cast<std::size_t>(std::max(1.0f, fits));
}

WrappedGrids wrapGrids(const Slot& slot, float width)
{
    WrappedGrids out;
    std::size_t columns = gridColumns(width);
    for (std::size_t r = 0; r < slot.size(); ++r) {
        const Row& row = slot[r];
        if (row.cells.empty()) {
            out.rows.push_back(row);
            out.firstCell.push_back(0);
            continue;
        }
        out.any = true;
        if (!row.list.empty()) out.columns[row.list] = columns;
        std::string name = row.id.empty() ? std::to_string(r) : row.id;
        for (std::size_t first = 0, line = 0; first < row.cells.size(); first += columns, ++line) {
            Row wrapped = row;
            wrapped.id = name + "." + std::to_string(line);
            wrapped.cells.assign(row.cells.begin() + static_cast<std::ptrdiff_t>(first),
                                 row.cells.begin() + static_cast<std::ptrdiff_t>(std::min(first + columns, row.cells.size())));
            out.rows.push_back(std::move(wrapped));
            out.firstCell.push_back(first);
        }
    }
    return out;
}

void placeCells(const Row& line, std::size_t firstCell, Rect frame, const std::optional<InList>& list, const std::string& key,
                const Source& source, float nestedRadius, Ms now, std::vector<Placed>& out, const MeasureText& measure)
{
    const Tokens& t = tokens();
    std::size_t columns = gridColumns(frame.width);
    float width = (frame.width - t.gridGap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    float radius = std::min({ nestedRadius, width / 2, frame.height / 2 });
    float pad = t.tilePadding;
    Source inside = source; // a cell's rows aren't lists of their own
    inside.scrolls = nullptr;
    inside.listDrag = nullptr;
    for (std::size_t i = 0; i < line.cells.size(); ++i) {
        const Cell& cell = line.cells[i];
        Rect box{ frame.x + static_cast<float>(i) * (width + t.gridGap), frame.y, width, frame.height };
        std::string cellKey = key + "/" + cell.id;
        std::size_t first = out.size();
        Rect inner{ box.x + pad, box.y + pad, std::max(0.0f, box.width - 2 * pad), std::max(0.0f, box.height - 2 * pad) };
        placeSlot({ cellKey, &cell.rows, inner }, inside, std::max(0.0f, radius - pad), now, out, measure);
        // The whole cell is under the pointer, not just its pieces: its picture's area covers it,
        // or, clickable anywhere, its button's (one hover for the whole card: decision 064).
        for (std::size_t p = first; p < out.size(); ++p) {
            if (cell.clickAnywhere && std::holds_alternative<Button>(out[p].item)) {
                out[p].hitFrame = box;
                out[p].litByRow = true;
            } else if (!cell.clickAnywhere && std::holds_alternative<Image>(out[p].item)) {
                out[p].hitFrame = box;
                break;
            }
        }
        // Its actions over its top-right corner, right to left from the edge.
        float x = box.x + box.width - pad;
        for (std::size_t a = cell.actions.size(); a-- > 0;) {
            const Item& action = cell.actions[a];
            float h = heightOf(action, box, 0);
            float w = fixedWidthOf(action, h, measure).value_or(h);
            x -= w;
            Rect at{ x, box.y + pad, w, h };
            out.push_back({ .key = cellKey + "/actions/" + std::to_string(a), .item = action, .frame = at, .hitFrame = at,
                            // A backed one is a disc, its hover too (decision 064); the rest nest in the cell's corner.
                            .cornerRadius = std::get_if<Button>(&action) && std::get<Button>(action).backed
                                                ? std::min(w, h) / 2 : std::min(std::max(0.0f, radius - pad), std::min(w, h) / 2),
                            .providerId = source.providerId, .activityId = source.activityId });
            x -= t.itemSpacing;
        }
        for (std::size_t p = first; p < out.size(); ++p) {
            Placed& placed = out[p];
            placed.group = cellKey;
            placed.openAction = cell.openAction;
            placed.listRow = box;
            placed.listRowRadius = radius;
            placed.accent = source.content ? source.content->accent : std::nullopt;
            if (list) {
                placed.list = list;
                placed.list->cell = firstCell + i;
                if (line.selectable && !cell.fixed) placed.list->item = cell.id; // decision 064
                placed.hitFrame = clippedTo(placed.hitFrame, list->viewport);
                placed.listRow = clippedTo(*placed.listRow, list->viewport);
            }
        }
    }
}

} // namespace cup
