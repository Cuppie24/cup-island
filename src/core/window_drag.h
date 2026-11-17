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

// Where it lands if let go now, from the pointer over the page as it's shown
// (`shown`: the last preview, the others already making room; before the
// first, the page as laid out). Over another card (a card) or row (a row):
// before it on its first half, after it on its second. Over no card, or in
// the room the carried thing left: `current` stays, so that room doesn't send
// the others back (decisions 135, 136).
DropTarget dropTarget(const WindowScene& shown, const WindowDrag& drag, const DropTarget& current);

// Where it was: a target that changes nothing.
DropTarget origin(const WindowDrag& drag);

// The page's content with it moved to `target`: laid out, the others make room.
// A card carried: without the dashed cards ("New group", decision 156).
WindowContent withMove(const WindowContent& content, const std::string& page, const WindowDrag& drag, const DropTarget& target);

// On `preview` (laid out from withMove), what's carried follows the pointer
// (Placed::carried) and a soft outline (kDropSpot) shows where it lands
// (decision 134). The rest glide as they make room, as any change to a
// window's page does (decision 132).
void follow(WindowScene& preview, const WindowDrag& drag);

// The drop outline's key on the page: under every card, never a row of one.
inline constexpr const char* kDropSpot = "drop/spot";

// Let go at `target`: Action{its move action, value = index, text = the
// card it lands in (a row)}, from the window. None: it lands where it was.
std::optional<Action> dropAction(const WindowScene& scene, const WindowDrag& drag, const DropTarget& target);

} // namespace cup
