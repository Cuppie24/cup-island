#pragma once
// A window card's menu (decision 142), pure: a card with WindowSection::menu
// has a "…" at its title row's end (kWindowCardMenu + its id); clicked, its
// menu opens under it, right edges together, the island's menu (decision 086)
// in the window. The app keeps which one is open (a core Menu: `row` is the
// card's id) and asks these where it is and what a click or key means.

#include "core/island.h"
#include "core/window_layout.h"

#include <optional>
#include <string>

namespace cup {

// The card a "…" button's action opens the menu of: its id. None: not one.
std::optional<std::string> cardMenuOf(const std::string& actionId);

// The menu for card `section` on the window's shown page, closed (nothing lit).
// None: no such card, or it has no menu.
std::optional<Menu> openCardMenu(const OpenWindow& window, const std::string& section);

// Laid out under its "…", in window coordinates, kept in the pane. None: its
// button isn't on the page any more (the card went), or its items changed.
std::optional<MenuScene> placeWindowMenu(const OpenWindow& window, const WindowScene& scene, const Menu& menu, Ms now,
                                         const MeasureText& measure);

// What choosing item `index` sends: its action, from the window. None: no
// such item, or it's dim.
std::optional<Action> menuChoice(const WindowScene& scene, const Menu& menu, std::size_t index);

} // namespace cup
