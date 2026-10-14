#include "core/lists.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

bool cardShown(const IslandState& s, const std::string& activityId) { return !s.homeOpen && s.expandedId == activityId; }

// One list's rows [first, end): placed into `plan` from `top` on. Returns the window's height.
float planList(const Slot& slot, const std::vector<float>& heights, const std::vector<float>& windowHeights, const ListState& state,
               std::size_t first, std::size_t end, float top, RowPlan& plan)
{
    const Tokens& t = tokens();
    std::size_t count = end - first;
    std::vector<float> natural(count); // each row's top within the list, unscrolled
    float content = 0;
    for (std::size_t i = 0; i < count; ++i) {
        natural[i] = content;
        content += heights[first + i] + (i + 1 < count ? t.rowSpacing : 0);
    }
    // The window: listVisibleRows rows (or the list's own visibleRows) of its
    // tallest row's height (half a row more hints there's more).
    // The tallest, not the first: a list may open with a shorter heading (Settings' "Dark").
    float tallest = *std::max_element(windowHeights.begin() + static_cast<std::ptrdiff_t>(first),
                                      windowHeights.begin() + static_cast<std::ptrdiff_t>(first + count));
    float rows = slot[first].visibleRows > 0 ? slot[first].visibleRows : t.listVisibleRows;
    float window = std::min(content, rows * (tallest + t.rowSpacing) - t.rowSpacing);
    float max = std::max(0.0f, content - window);

    const std::string& id = slot[first].list;
    std::string key = listKey(state.activityId, id);
    float offset = 0;
    if (state.scrolls) {
        if (auto it = state.scrolls->find(key); it != state.scrolls->end()) {
            offset = it->second.offset;
            if (auto r = it->second.reveal; r && *r < count) { // the least scrolling that shows it
                float rowTop = natural[*r], rowBottom = rowTop + heights[first + *r];
                if (rowTop < offset) offset = rowTop;
                if (rowBottom > offset + window) offset = rowBottom - window;
            }
        }
    }
    offset = std::clamp(offset, 0.0f, max);

    // Dragged: it follows the pointer; the others close up around where it would land.
    std::vector<float> tops = natural;
    std::optional<std::size_t> from, to;
    const ListDrag* drag = state.drag;
    if (drag && drag->activityId == state.activityId && drag->list == id && drag->row < count &&
        !slot[first + drag->row].moveAction.empty()) {
        std::size_t f = drag->row;
        float h = heights[first + f];
        float dragTop = std::clamp(natural[f] + drag->y - drag->grabY, 0.0f, content - h);
        // It moves only among its sortGroup's neighbours.
        std::size_t lo = f, hi = f;
        int group = slot[first + f].sortGroup;
        while (lo > 0 && slot[first + lo - 1].sortGroup == group && !slot[first + lo - 1].moveAction.empty()) --lo;
        while (hi + 1 < count && slot[first + hi + 1].sortGroup == group && !slot[first + hi + 1].moveAction.empty()) ++hi;
        std::size_t target = lo;
        // It goes past a row above once its top reaches that row's top, and
        // past one below once its bottom reaches that row's bottom. Rows of one
        // height swap where their centres cross; a taller one (a wrapped to-do,
        // decision 045) can still pass a shorter one at either end, where
        // its centre never could. A tie (pushed against an end) counts toward
        // the way it moved.
        for (std::size_t i = lo; i <= hi; ++i) {
            if (i < f && natural[i] < dragTop) ++target;                                  // still above it
            if (i > f && natural[i] + heights[first + i] <= dragTop + h) ++target;        // passed: now above it
        }
        // The new order, stacked from the run's first row.
        std::vector<std::size_t> order;
        for (std::size_t i = lo; i <= hi; ++i)
            if (i != f) order.push_back(i);
        order.insert(order.begin() + static_cast<std::ptrdiff_t>(target - lo), f);
        float y = natural[lo];
        for (std::size_t i : order) {
            tops[i] = y;
            y += heights[first + i] + t.rowSpacing;
        }
        tops[f] = dragTop;
        from = f;
        to = target;
    }

    for (std::size_t i = 0; i < count; ++i) {
        RowSpot& spot = plan.rows[first + i];
        spot.y = top + tops[i] - offset;
        spot.list = InList{ .id = id, .row = i, .viewport = { 0, top, 0, window }, .offset = offset, .max = max,
                            .moveAction = slot[first + i].moveAction, .dragged = from == i, .dropAt = from == i ? *to : i };
    }
    return window;
}

} // namespace

std::string listKey(const std::string& activityId, const std::string& list) { return activityId + "/" + list; }

RowPlan planRows(const Slot& slot, const std::vector<float>& heights, const ListState& state, const std::vector<float>* windowHeights)
{
    const Tokens& t = tokens();
    RowPlan plan;
    plan.rows.resize(slot.size());
    float y = 0;
    for (std::size_t r = 0; r < slot.size();) {
        if (slot[r].list.empty()) {
            plan.rows[r].y = y;
            y += heights[r] + t.rowSpacing;
            ++r;
            continue;
        }
        std::size_t end = r;
        while (end < slot.size() && slot[end].list == slot[r].list) ++end;
        y += planList(slot, heights, windowHeights ? *windowHeights : heights, state, r, end, y, plan) + t.rowSpacing;
        r = end;
    }
    plan.height = slot.empty() ? 0 : y - t.rowSpacing;
    return plan;
}

void scrollList(IslandState& s, const ListScrolled& e, Ms now)
{
    ListScroll& scroll = s.scrolls[listKey(e.activityId, e.list)];
    scroll.offset = std::clamp(e.from + e.delta, 0.0f, std::max(0.0f, e.max));
    scroll.reveal.reset(); // the user scrolls now
    s.lastPointerAt = now;
}

void revealListRow(IslandState& s, const ListRevealed& e) { s.scrolls[listKey(e.activityId, e.list)].reveal = e.row; }

void startListDrag(IslandState& s, const ListDragStarted& e, Ms now)
{
    if (!cardShown(s, e.activityId) || s.dialog) return;
    s.listDrag = ListDrag{ e.activityId, e.list, e.row, e.y, e.y };
    s.lastPointerAt = now;
}

void moveListDrag(IslandState& s, const ListDragMoved& e, Ms now)
{
    if (!s.listDrag) return;
    s.listDrag->y = e.y;
    s.lastPointerAt = now;
}

} // namespace cup
