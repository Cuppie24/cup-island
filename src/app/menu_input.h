#pragma once
// The app's side of a row's menu (decision 086): the pointer and keys on it
// and the items' shortcuts. What it does is core's (core/menu: when it opens
// and closes, which item is lit, where it goes); this turns input into those
// events and sends a chosen item's action, like the keyboard (keyboard_input).

#include "core/island.h"
#include "core/layout.h"
#include "core/provider.h"
#include "platform/win.h"

#include <functional>
#include <string>

namespace cup::app {

struct MenuHooks {
    std::function<const IslandState&()> state;
    std::function<const Scene&()> scene; // the island's
    std::function<void(const Event&, const char* reason)> dispatch;
    std::function<void(const Action&)> act; // a chosen item's action: to its provider
};
void setMenuHooks(MenuHooks hooks);

// A right-click (pill coordinates): the menu of the row under it, there.
void menuRightClick(Vec2 point);
// The keys' way in (Right, the Menu key, Shift+F10): the lit row's menu,
// beside it. False: that row has none, and the key does what it did before.
bool openMenuByKeys();

// The pointer, while a menu is open. A press on it is its own (an item is
// chosen on release over the same one); anywhere else it only closes the
// menu. True: the menu took it, nothing under it sees it.
bool menuPress(Vec2 point);
bool menuRelease(Vec2 point);
void menuPointerMoved(Vec2 point); // lights the item under it

// A key, menu open or not: an item's shortcut on the lit row, or, with a
// menu open, Up, Down, Home, End, a letter, Enter, Space, Esc or Left. True:
// it was the menu's. Call clearCharSwallow() on every key first.
bool menuKey(WPARAM key);
// The WM_CHAR that follows a key the menu took (Enter, Space, a letter,
// Ctrl+C) isn't typed into the field.
bool swallowChar();
void clearCharSwallow();

} // namespace cup::app
