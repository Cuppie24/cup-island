#pragma once
// A row's context menu (decision 086). The provider lists its items
// (Row::menu); the host does the rest, here: when it opens and closes, which
// item is lit, where it goes, what it looks like, and the items' shortcuts.
// One at a time, on the island's shown card. Pure.

#include "core/island.h"
#include "core/layout.h"
#include "core/slot_layout.h"

#include <optional>
#include <string>

namespace cup {

std::string rowKeyOf(const std::string& elementKey); // "expandedCenter/3/1" → "expandedCenter/3"
// The row a key names in a card's content ("<slot>/<index or Row::id>"); none: gone.
const Row* rowNamed(const Content& content, const std::string& rowKey);

// Part of reduce(). It opens only on the island's shown card, not under a
// dialog, on a row that has a menu; by the keys with its first item lit.
// Using it counts as a touch: the card doesn't time out while it's open.
void openMenu(IslandState& state, const MenuOpened& e, Ms now);
void lightMenu(IslandState& state, const MenuLit& e, Ms now); // a dim item or one past the end: nothing lit
void closeMenu(IslandState& state);
// After every event: it goes with its card, under a dialog, and when its row
// is gone or its items changed (a new query's rows). A republish that keeps
// them (an icon arriving) leaves it open.
void dropStaleMenu(IslandState& state);

// The keys in an open menu. Up and Down (step -1, +1): the next item that
// can be chosen, round from the last to the first; nothing lit, Down is the
// first and Up the last. Home and End: the first and last. A letter: the
// next item after the lit one whose label starts with it (A..Z, either case).
std::optional<std::size_t> menuStep(const Menu& menu, int step);
std::optional<std::size_t> menuEnd(const Menu& menu, bool last);
std::optional<std::size_t> menuLetter(const Menu& menu, char letter);

// An item's shortcut (MenuItem::chord): the enabled item in that row's menu
// with these keys, if any. The app asks it for the lit row on every key,
// menu open or not.
const MenuItem* itemForChord(const Content& content, const std::string& rowKey, const Chord& chord);

// Where the row is on the card: its elements' band (a list row's whole
// background, before its list's window clipped it). Empty: not on screen.
Rect rowFrame(const Scene& scene, const std::string& rowKey);

// Lays the menu out. `row`: its row's frame; `bounds`: where it may go, the
// render layer in pill coordinates (it may leave the island, not the
// screen). A right-click: its top-left at the point, or its right edge there
// where there's no room to the right, moved up to fit. By the keys: beside the
// row, menuGap from its right edge, top to top; on the row's left side when
// there's no room on the right.
MenuScene placeMenu(const Menu& menu, const Source& source, Rect row, Rect bounds, Ms now, const MeasureText& measure);

// The item under `point` (pill coordinates), if it can be chosen.
std::optional<std::size_t> menuItemAt(const MenuScene& menu, Vec2 point);

} // namespace cup
