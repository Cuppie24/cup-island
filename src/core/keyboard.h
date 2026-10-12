#pragma once
// Moving around the island with the arrow keys (decision 031). Pure: what
// the keyboard can land on in a Scene, and where an arrow goes from where it
// is. The app keeps which one has focus and lights it like a hover; Enter or
// Space clicks it.

#include "core/layout.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

enum class Arrow { Up, Down, Left, Right };

// Something the keyboard can land on: a button, a text field or clickable
// art; any Home tile, even one a click does nothing with (so the arrows
// never skip a row of static widgets); a tab that isn't the one shown.
struct Focusable {
    std::string id;             // "el:<element key>", "tile:<panel id>", "tab:<activity id>" ("tab:" = Home)
    Rect frame;                 // where it is, to aim arrows (a list row out of its window: where it would be)
    Vec2 point;                 // where Enter clicks: inside its hit area, so hit-testing finds it
    std::optional<InList> list; // in a scrolling list: scroll it into view before landing (ListRevealed)
    std::string activityId;
    float radius = 0;           // its corners, for the focus ring: those of its hover shape
};

// Everything focusable in reading order (top to bottom, then left to right).
std::vector<Focusable> focusables(const Scene& scene);

std::optional<Focusable> findFocusable(const Scene& scene, const std::string& id);

// Where `arrow` goes from `current`: the nearest one that way. Left and
// right stay on its row; up and down favour what's in line with it. Nothing
// that way: nothing. `current` not on screen
// (or empty): the first, so the first press shows where focus is.
std::optional<Focusable> moveFocus(const Scene& scene, const std::string& current, Arrow arrow);

// Up or down from the last stop that way in a scrolling list that can still
// scroll that way: the list's scroll (a scrollStep), with focus staying put,
// so rows with nothing to click (Settings' list of keys) come into view.
// Nothing: move focus instead (moveFocus).
std::optional<ListScrolled> scrollInstead(const Scene& scene, const std::string& current, Arrow arrow);

// It's in view: not a list row scrolled out of its window. The focus ring
// shows only then; it would be drawn outside the list.
bool inView(const Focusable& focusable);

// Tab: the next one in reading order, round to the first after the last.
// Shift+Tab: the one before, round to the last before the first.
// `current` not on screen (or empty): the first, either way.
std::optional<Focusable> nextFocus(const Scene& scene, const std::string& current);
std::optional<Focusable> previousFocus(const Scene& scene, const std::string& current);

// Home and End: the first or the last one in reading order.
std::optional<Focusable> firstFocus(const Scene& scene);
std::optional<Focusable> lastFocus(const Scene& scene);

// The keys 1 to 9 on Home: its `n`th widget (from 1), in publish order. Nothing past the last.
std::optional<Focusable> homeWidget(const Scene& scene, std::size_t n);

// Ctrl+Tab: the id of the tab after the one shown ("" = Home), round to
// the first after the last; Ctrl+Shift+Tab: the one before, round to the
// last. Home when none is shown (a dormant panel's card). Nothing without
// a tab strip.
std::optional<std::string> nextTab(const Scene& scene);
std::optional<std::string> previousTab(const Scene& scene);

} // namespace cup
