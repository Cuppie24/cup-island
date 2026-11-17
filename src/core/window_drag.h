#pragma once
// Dragging on a window's page (decision 129), pure: a task to another place
// in its card or into another card, a card (by its handle) to another place
// among the cards. While it's carried, the page is laid out as if it were
// already where it would land (the others make room) and the carried thing
// follows the pointer; let go, its provider gets its move action. The app
// keeps the drag (app/provider_windows) and asks these what it means.

#include "core/provider.h"
#include "core/window_layout.h"

#include <optional>
#include <string>

namespace cup {

struct WindowDrag {
    Movable what;
    Vec2 grab; // where on it it was picked up, from its frame's corner
    Vec2 at;   // the pointer now, page coordinates
    bool operator==(const WindowDrag&) const = default;
};

// Where it would land: a card's id and an index among its movable rows (a
// row), or "" and an index among the page's movable cards (a card).
struct DropTarget {
    std::string section;
    std::size_t index = 0;
    bool operator==(const DropTarget&) const = default;
};

// What a press at `point` (window coordinates) would pick up: a card by its
// handle, else a movable row. None: nothing there moves.
std::optional<Movable> movableAt(const WindowScene& scene, Vec2 point);

// Picked up at `point` (window coordinates): the drag, not yet moved.
WindowDrag startDrag(const WindowScene& scene, const Movable& what, Vec2 point);

// Where it lands if let go now, worked out on `scene` as laid out without the
// drag (so what makes room doesn't move the target). A row over no card goes
// back where it was.
DropTarget dropTarget(const WindowScene& scene, const WindowDrag& drag);

// Where it was: a target that changes nothing.
DropTarget origin(const WindowDrag& drag);

// The page's content with it moved to `target`: laid out, the others make room.
WindowContent withMove(const WindowContent& content, const std::string& page, const WindowDrag& drag, const DropTarget& target);

// On `preview` (laid out from withMove), what's carried follows the pointer.
void follow(WindowScene& preview, const WindowDrag& drag);

// Let go at `target`: Action{its move action, value = index, text = the
// card it lands in (a row)}, from the window. None: it lands where it was.
std::optional<Action> dropAction(const WindowScene& scene, const WindowDrag& drag, const DropTarget& target);

} // namespace cup
