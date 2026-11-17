#include "core/window_drag.h"

#include "core/window_page.h"

#include <algorithm>

namespace cup {
namespace {

Vec2 centreOf(Rect r) { return { r.x + r.width / 2, r.y + r.height / 2 }; }

// Where in `items` the index-th of those `moves` says move goes, the carried one taken out already:
// before it, or after the last one, or at `fallback` if there's none.
template <typename T, typename Moves>
std::size_t insertAt(const std::vector<T>& items, std::size_t index, Moves moves, std::size_t fallback)
{
    std::size_t seen = 0;
    std::optional<std::size_t> last;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (!moves(items[i])) continue;
        if (seen++ == index) return i;
        last = i;
    }
    return last ? *last + 1 : fallback;
}

} // namespace

std::optional<Movable> movableAt(const WindowScene& s, Vec2 point)
{
    if (s.dialog || !s.pane.contains(point)) return std::nullopt;
    Vec2 at = toPage(s, point);
    for (const Placed& p : s.page) // a card's handle first: it's over the card's top row
        if (p.key.size() > 7 && p.key.ends_with("/handle") && p.hitFrame.contains(at)) {
            std::string card = p.key.substr(0, p.key.size() - 7);
            for (const Movable& m : s.movables)
                if (m.card && m.key == card) return m;
        }
    // A card's header (decision 143), but not a button on it (its "…", a delete): that's the button's.
    bool onButton = false;
    for (const Placed& p : s.page)
        if (auto* b = std::get_if<Button>(&p.item); b && b->enabled && b->actionId != kWindowDragHandle && p.hitFrame.contains(at)) onButton = true;
    if (!onButton)
        for (const Movable& m : s.movables)
            if (m.card && m.grip.contains(at)) return m;
    for (const Movable& m : s.movables)
        if (!m.card && m.frame.contains(at)) return m;
    return std::nullopt;
}

WindowDrag startDrag(const WindowScene& s, const Movable& what, Vec2 point)
{
    Vec2 at = toPage(s, point);
    return { what, { at.x - what.frame.x, at.y - what.frame.y }, at };
}

DropTarget origin(const WindowDrag& d) { return { d.what.card ? "" : d.what.section, d.what.index }; }

namespace {

// A card's place (decision 135): over another card, before it on its first half (its top; its
// left when cards sit side by side on that row), after it on its second. Over none: as it was.
DropTarget cardTarget(const WindowScene& shown, const WindowDrag& d, const DropTarget& current)
{
    std::vector<Rect> others; // the other movable cards, in order, where they show now
    for (const Movable& m : shown.movables)
        if (m.card && m.action != d.what.action) others.push_back(m.frame);
    for (std::size_t k = 0; k < others.size(); ++k) {
        Rect r = others[k];
        if (!r.contains(d.at)) continue;
        // Another card shown on its row (where the carried one lands): the cards run left to right there.
        // Not a dashed one ("New group"): it's left out while a card is carried (decision 156).
        bool beside = false;
        for (const Placed& p : shown.page) {
            auto* fill = std::get_if<GroupFill>(&p.item);
            if (!fill || fill->dashed || p.carried) continue;
            Rect f = p.frame;
            bool sameRow = f.y < r.y + r.height && f.y + f.height > r.y;
            bool apart = f.x >= r.x + r.width || f.x + f.width <= r.x;
            if (sameRow && apart) beside = true;
        }
        Vec2 c = centreOf(r);
        bool past = beside ? d.at.x > c.x : d.at.y > c.y;
        return { "", k + (past ? 1 : 0) };
    }
    return current;
}

// A row's place (decision 136), the same way, in the card under the pointer: over another row,
// before it on its top half, after it on its bottom half; above its rows (its name, "Add task"),
// first; below them, last; in the gap the carried row leaves, as it was. Over no card: as it was.
DropTarget rowTarget(const WindowScene& shown, const WindowDrag& d, const DropTarget& current)
{
    for (const DropZone& zone : shown.dropZones) {
        if (!zone.frame.contains(d.at)) continue;
        std::vector<Rect> rows; // its other movable rows, in order, where they show now
        for (const Movable& m : shown.movables)
            if (!m.card && m.section == zone.section && m.action != d.what.action) rows.push_back(m.frame);
        if (rows.empty() || d.at.y < rows.front().y) return { zone.section, 0 };
        if (d.at.y >= rows.back().y + rows.back().height) return { zone.section, rows.size() };
        for (std::size_t k = 0; k < rows.size(); ++k)
            if (d.at.y >= rows[k].y && d.at.y < rows[k].y + rows[k].height)
                return { zone.section, k + (d.at.y > centreOf(rows[k]).y ? 1 : 0) };
        if (current.section == zone.section) return current; // the room it leaves: it stays there
        std::size_t index = 0; // a gap in another card: between the rows it's between
        while (index < rows.size() && rows[index].y + rows[index].height <= d.at.y) ++index;
        return { zone.section, index };
    }
    return current;
}

} // namespace

DropTarget dropTarget(const WindowScene& shown, const WindowDrag& d, const DropTarget& current)
{
    return d.what.card ? cardTarget(shown, d, current) : rowTarget(shown, d, current);
}

WindowContent withMove(const WindowContent& content, const std::string& pageId, const WindowDrag& d, const DropTarget& t)
{
    WindowContent moved = content;
    auto page = std::find_if(moved.pages.begin(), moved.pages.end(), [&](const WindowPage& p) { return p.id == pageId; });
    if (page == moved.pages.end()) return moved;
    auto& sections = page->sections;
    auto sectionWith = [&](const std::string& id) {
        return std::find_if(sections.begin(), sections.end(), [&](const WindowSection& sec) { return sec.id == id; });
    };
    if (d.what.card) {
        auto from = sectionWith(d.what.section);
        if (from == sections.end()) return moved;
        WindowSection carried = std::move(*from);
        sections.erase(from);
        // A dashed card ("New group") is a button, not a card among them: left out while one is
        // carried, so the cards make room only for each other (decision 156); it's back on drop.
        std::erase_if(sections, [](const WindowSection& sec) { return sec.dashed; });
        std::size_t at = insertAt(sections, t.index, [](const WindowSection& sec) { return !sec.moveAction.empty(); }, sections.size());
        sections.insert(sections.begin() + static_cast<std::ptrdiff_t>(at), std::move(carried));
        return moved;
    }
    auto from = sectionWith(d.what.section), to = sectionWith(t.section);
    if (from == sections.end() || to == sections.end()) return moved;
    auto& source = from->rows;
    auto row = std::find_if(source.begin(), source.end(), [&](const Row& r) { return r.moveAction == d.what.action; });
    if (row == source.end()) return moved;
    Row carried = std::move(*row);
    source.erase(row);
    // In another card, a name of its own: a provider's row ids are its card's (Pomodoro's tasks are
    // numbered per group), so it could share one with a row there, and their items' keys with it:
    // both followed the pointer, the outline spanned both, the card's rows looked gone (decision 138).
    if (to != from) carried.id = d.what.section + ":" + (carried.id.empty() ? d.what.action : carried.id);
    auto& target = to->rows;
    std::size_t at = insertAt(target, t.index, [](const Row& r) { return !r.moveAction.empty(); }, target.size());
    target.insert(target.begin() + static_cast<std::ptrdiff_t>(at), std::move(carried));
    return moved;
}

void follow(WindowScene& preview, const WindowDrag& d)
{
    auto now = std::find_if(preview.movables.begin(), preview.movables.end(), [&](const Movable& m) { return m.action == d.what.action; });
    if (now == preview.movables.end()) return;
    Vec2 delta{ d.at.x - d.grab.x - now->frame.x, d.at.y - d.grab.y - now->frame.y };
    auto shift = [&](Rect& r) { r.x += delta.x, r.y += delta.y; };
    std::string prefix = now->key + "/";
    // Where it lands, before it's moved to the pointer: a soft outline there (decision 134), as
    // round as the card, or the row's lit background.
    Placed spot;
    spot.key = kDropSpot;
    spot.item = GroupFill{ .outline = true };
    spot.frame = now->frame;
    for (Placed& p : preview.page) {
        if (p.key.rfind(prefix, 0) != 0) continue;
        if (now->card && std::holds_alternative<GroupFill>(p.item)) spot.cornerRadius = p.cornerRadius;
        if (!now->card && p.listRow) spot.cornerRadius = p.listRowRadius;
        spot.providerId = p.providerId;
        spot.activityId = p.activityId;
        shift(p.frame);
        shift(p.hitFrame);
        p.carried = true;
        if (p.listRow) shift(*p.listRow);
        p.carried = true; // at the pointer at once, not easing after it
    }
    shift(now->frame);
    preview.page.insert(preview.page.begin(), std::move(spot)); // under everything
}

std::optional<Action> dropAction(const WindowScene& s, const WindowDrag& d, const DropTarget& t)
{
    if (t == origin(d)) return std::nullopt;
    return Action{ .providerId = s.providerId, .actionId = d.what.action, .value = static_cast<float>(t.index), .text = t.section,
                   .windowId = s.windowId };
}

} // namespace cup
