#include "app/selection_input.h"

#include "core/selection.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cup::app {
namespace {

SelectionHooks g_hooks;

// A box drawn round things (decision 064): from where it started, adding to
// what was picked before it when Ctrl was held.
struct Box {
    Vec2 from;
    std::string activityId, list;
    std::vector<std::string> before;
};
std::optional<Box> g_box;

// A press on a thing already picked, with nothing held: on release it's the
// only one picked (a press may still start a drag one day); on its name, a
// slow second click renames it.
struct Pending {
    std::string activityId, list, item;
    std::string renameKey; // the name's element key, if it was the only one picked and the press was on its name
};
std::optional<Pending> g_pending;
std::string g_renameKey;   // waiting for the double-click time to pass

const IslandState& state() { return g_hooks.state(); }
const Scene& scene() { return g_hooks.scene(); }
bool held(int key) { return GetKeyState(key) < 0; }

bool selectable(const Placed* p) { return p && p->list && !p->list->item.empty(); }

// The things in a list on the scene and where they are (each once).
std::vector<std::pair<std::string, Rect>> itemsOf(const std::string& list)
{
    std::vector<std::pair<std::string, Rect>> out;
    for (const Placed& p : scene().elements) {
        if (!selectable(&p) || p.list->id != list || !p.listRow) continue;
        if (std::none_of(out.begin(), out.end(), [&](const auto& e) { return e.first == p.list->item; })) out.push_back({ p.list->item, *p.listRow });
    }
    return out;
}

const Placed* nameOf(const std::string& item)
{
    for (const Placed& p : scene().elements)
        if (selectable(&p) && p.list->item == item)
            if (auto* text = std::get_if<Text>(&p.item); text && !text->editAction.empty()) return &p;
    return nullptr;
}

bool onlyPicked(const std::string& list, const std::string& item)
{
    const auto& s = state().selection;
    return s && s->list == list && s->ids.size() == 1 && s->ids[0] == item;
}

bool overlaps(Rect a, Rect b) { return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height; }

} // namespace

void setSelectionHooks(SelectionHooks hooks) { g_hooks = std::move(hooks); }

bool selectionPressed(Vec2 point, const Placed* element)
{
    if (element && std::holds_alternative<Button>(element->item)) return false; // its own click (Remove, Show in folder)
    g_pending.reset();
    // The thing under the pointer: its picture's area covers it, so not just what hitTest found.
    const Placed* under = nullptr;
    for (const Placed& p : scene().elements)
        if (p.hitFrame.contains(point) && selectable(&p)) {
            under = &p;
            break;
        }
    bool ctrl = held(VK_CONTROL), shift = held(VK_SHIFT);
    if (under) {
        const std::string& list = under->list->id;
        const std::string& item = under->list->item;
        bool picked = isSelected(state(), under->activityId, list, item);
        if (ctrl || shift || !picked) {
            g_hooks.dispatch(ItemSelected{ under->activityId, list, item, shift, ctrl }, "pick");
            return true;
        }
        // Already picked: decided on release. On the name of the only one picked, a slow second click renames it.
        bool onName = element && std::holds_alternative<Text>(element->item) && !std::get<Text>(element->item).editAction.empty();
        g_pending = Pending{ under->activityId, list, item, onName && onlyPicked(list, item) ? element->key : std::string() };
        return true;
    }
    // The empty part of a selectable list: a box starts there (decision 064).
    for (const Placed& p : scene().elements) {
        if (!selectable(&p) || !p.list->viewport.contains(point)) continue;
        Box box{ point, p.activityId, p.list->id, {} };
        if (ctrl && state().selection && state().selection->list == box.list) box.before = state().selection->ids;
        else g_hooks.dispatch(SelectionCleared{}, "pick none");
        g_box = box;
        return true;
    }
    return false;
}

void selectionMoved(Vec2 point)
{
    if (!g_box) return;
    Rect box{ std::min(point.x, g_box->from.x), std::min(point.y, g_box->from.y), std::abs(point.x - g_box->from.x), std::abs(point.y - g_box->from.y) };
    g_hooks.showBox(box);
    std::vector<std::string> ids = g_box->before;
    for (const auto& [item, frame] : itemsOf(g_box->list))
        if (overlaps(box, frame) && std::find(ids.begin(), ids.end(), item) == ids.end()) ids.push_back(item);
    if (!state().selection || state().selection->ids.size() != ids.size() || !std::is_permutation(ids.begin(), ids.end(), state().selection->ids.begin()))
        g_hooks.dispatch(SelectionSet{ g_box->activityId, g_box->list, ids }, "pick box");
}

bool selectionReleased(Vec2 point, bool inside)
{
    if (g_box) {
        g_box.reset();
        g_hooks.showBox(std::nullopt);
        return true;
    }
    if (!g_pending) return false;
    Pending pending = *std::exchange(g_pending, std::nullopt);
    if (!inside) return true;
    g_hooks.dispatch(ItemSelected{ pending.activityId, pending.list, pending.item }, "pick");
    // Rename only once no second click makes it a double-click (which opens it).
    const Placed* name = pending.renameKey.empty() ? nullptr : nameOf(pending.item);
    if (name && name->hitFrame.contains(point)) {
        g_renameKey = pending.item;
        g_hooks.renameTimer(Ms{ GetDoubleClickTime() });
    }
    return true;
}

void renameDue()
{
    std::string item = std::exchange(g_renameKey, std::string());
    if (item.empty()) return;
    if (const Placed* name = nameOf(item); name && onlyPicked(name->list->id, item)) g_hooks.rename(*name);
}

void cancelRename()
{
    g_renameKey.clear();
    g_hooks.renameTimer(std::nullopt);
}

bool selectionKey(WPARAM key, bool ctrl)
{
    if (mode(state()) != Mode::Expanded) return false;
    // The card's selectable list, if it has one on screen.
    const Placed* any = nullptr;
    for (const Placed& p : scene().elements)
        if (selectable(&p)) {
            any = &p;
            break;
        }
    if (!any) return false;
    const auto& picked = state().selection;
    auto card = shownCard(state());
    if (ctrl && key == 'A') { // everything in the list
        g_hooks.dispatch(SelectionSet{ any->activityId, any->list->id, selectableItems(state(), any->activityId, any->list->id) }, "pick all");
        return true;
    }
    if (key == VK_DELETE && picked && card) {
        const Panel* panel = findPanel(state(), card->activityId);
        const Activity* activity = findActivity(state(), card->activityId);
        const Content* content = activity ? &activity->content : panel ? &panel->content : nullptr;
        if (content && !content->deleteAction.empty()) g_hooks.act({ card->providerId, card->activityId, content->deleteAction });
        return true;
    }
    if ((key == VK_RETURN || key == VK_F2) && picked && picked->ids.size() == 1) {
        for (const Placed& p : scene().elements) {
            if (!selectable(&p) || p.list->item != picked->ids[0]) continue;
            if (key == VK_RETURN && !p.openAction.empty()) {
                g_hooks.act({ p.providerId, p.activityId, p.openAction });
                return true;
            }
        }
        if (key == VK_F2) {
            if (const Placed* name = nameOf(picked->ids[0])) g_hooks.rename(*name);
            return true;
        }
    }
    return false;
}

void focusLanded(const Placed& on, bool shift, bool ctrl)
{
    if (!selectable(&on) || ctrl) return; // Ctrl+arrows move focus without picking, like Explorer
    g_hooks.dispatch(ItemSelected{ on.activityId, on.list->id, on.list->item, shift, false }, "pick by keys");
}

} // namespace cup::app
