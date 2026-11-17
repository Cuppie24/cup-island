#include "core/window_drag.h"

#include <algorithm>

namespace cup {
namespace {

Vec2 centreOf(Rect r) { return { r.x + r.width / 2, r.y + r.height / 2 }; }

// Where the carried thing's middle is now.
Vec2 carriedCentre(const WindowDrag& d)
{
    return { d.at.x - d.grab.x + d.what.frame.width / 2, d.at.y - d.grab.y + d.what.frame.height / 2 };
}

// Reading order on a grid of cards: a card on a row above, or left of the point on its row.
bool before(Rect card, Vec2 point)
{
    Vec2 c = centreOf(card);
    bool sameRow = point.y >= card.y && point.y < card.y + card.height;
    return sameRow ? c.x < point.x : c.y < point.y;
}

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

DropTarget dropTarget(const WindowScene& s, const WindowDrag& d)
{
    Vec2 c = carriedCentre(d);
    if (d.what.card) {
        std::size_t index = 0;
        for (const Movable& m : s.movables)
            if (m.card && m.key != d.what.key && before(m.frame, c)) ++index;
        return { "", index };
    }
    for (const DropZone& zone : s.dropZones) {
        if (!zone.frame.contains(c)) continue;
        std::size_t index = 0;
        for (const Movable& m : s.movables)
            if (!m.card && m.section == zone.section && m.key != d.what.key && centreOf(m.frame).y < c.y) ++index;
        return { zone.section, index };
    }
    return origin(d); // over no card: back where it was
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
    for (Placed& p : preview.page) {
        if (p.key.rfind(prefix, 0) != 0) continue;
        shift(p.frame);
        shift(p.hitFrame);
        p.carried = true;
        if (p.listRow) shift(*p.listRow);
    }
    shift(now->frame);
}

std::optional<Action> dropAction(const WindowScene& s, const WindowDrag& d, const DropTarget& t)
{
    if (t == origin(d)) return std::nullopt;
    return Action{ .providerId = s.providerId, .actionId = d.what.action, .value = static_cast<float>(t.index), .text = t.section,
                   .windowId = s.windowId };
}

} // namespace cup
