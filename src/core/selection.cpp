#include "core/selection.h"

#include <algorithm>

namespace cup {
namespace {

bool shown(const IslandState& s, const std::string& activityId) { return !s.homeOpen && s.expandedId == activityId; }

const Content* contentOf(const IslandState& s, const std::string& id)
{
    if (const Activity* a = findActivity(s, id)) return &a->content;
    if (const Panel* p = findPanel(s, id)) return &p->content;
    return nullptr;
}

bool contains(const std::vector<std::string>& ids, const std::string& id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); }

// `wanted`, in the list's order and without strays.
std::vector<std::string> ordered(const std::vector<std::string>& items, const std::vector<std::string>& wanted)
{
    std::vector<std::string> out;
    for (const std::string& id : items)
        if (contains(wanted, id)) out.push_back(id);
    return out;
}

} // namespace

std::vector<std::string> selectableItems(const IslandState& s, const std::string& activityId, const std::string& list)
{
    std::vector<std::string> ids;
    const Content* c = contentOf(s, activityId);
    if (!c) return ids;
    for (const Row& r : c->expandedCenter) {
        if (r.list != list || !r.selectable) continue;
        if (r.cells.empty()) {
            if (!r.id.empty()) ids.push_back(r.id);
            continue;
        }
        for (const Cell& cell : r.cells)
            if (!cell.fixed) ids.push_back(cell.id);
    }
    return ids;
}

void selectItem(IslandState& s, const ItemSelected& e, Ms now)
{
    if (!shown(s, e.activityId)) return;
    std::vector<std::string> items = selectableItems(s, e.activityId, e.list);
    if (!contains(items, e.id)) return;
    s.lastPointerAt = now;
    bool same = s.selection && s.selection->activityId == e.activityId && s.selection->list == e.list;
    Selection next{ e.activityId, e.list, {}, e.id };
    if (e.range && same && contains(items, s.selection->anchor)) {
        // From the anchor to it, inclusive; with Ctrl too, added to what's there. The anchor stays.
        auto a = std::find(items.begin(), items.end(), s.selection->anchor) - items.begin();
        auto b = std::find(items.begin(), items.end(), e.id) - items.begin();
        std::vector<std::string> range(items.begin() + std::min(a, b), items.begin() + std::max(a, b) + 1);
        if (e.toggle) range.insert(range.end(), s.selection->ids.begin(), s.selection->ids.end());
        next.ids = ordered(items, range);
        next.anchor = s.selection->anchor;
    } else if (e.toggle && same) {
        next.ids = s.selection->ids;
        if (contains(next.ids, e.id)) std::erase(next.ids, e.id);
        else next.ids = ordered(items, [&] { auto ids = next.ids; ids.push_back(e.id); return ids; }());
    } else {
        next.ids = { e.id };
    }
    if (next.ids.empty()) s.selection.reset();
    else s.selection = next;
}

void setSelection(IslandState& s, const SelectionSet& e, Ms now)
{
    if (!shown(s, e.activityId)) return;
    std::vector<std::string> ids = ordered(selectableItems(s, e.activityId, e.list), e.ids);
    s.lastPointerAt = now;
    if (ids.empty()) {
        s.selection.reset();
        return;
    }
    std::string anchor = s.selection && contains(ids, s.selection->anchor) ? s.selection->anchor : ids.front();
    s.selection = Selection{ e.activityId, e.list, ids, anchor };
}

void settleSelection(IslandState& s)
{
    if (!s.selection) return;
    if (!shown(s, s.selection->activityId)) {
        s.selection.reset();
        return;
    }
    std::vector<std::string> items = selectableItems(s, s.selection->activityId, s.selection->list);
    std::erase_if(s.selection->ids, [&](const std::string& id) { return !contains(items, id); }); // removed: no longer picked
    if (s.selection->ids.empty()) s.selection.reset();
}

bool isSelected(const IslandState& s, const std::string& activityId, const std::string& list, const std::string& id)
{
    return s.selection && s.selection->activityId == activityId && s.selection->list == list && contains(s.selection->ids, id);
}

void markSelection(const IslandState& s, Scene& scene)
{
    if (!s.selection) return;
    std::vector<std::string> marked; // one background per row or cell, not per piece of it
    for (const Placed& p : scene.elements) {
        if (!p.list || p.list->item.empty() || !p.listRow || p.activityId != s.selection->activityId) continue;
        if (!isSelected(s, p.activityId, p.list->id, p.list->item) || contains(marked, p.list->item)) continue;
        marked.push_back(p.list->item);
        scene.selected.push_back(*p.listRow);
        scene.selectedRadius = p.listRowRadius;
    }
}

} // namespace cup
