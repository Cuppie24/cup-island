#pragma once
// The app's side of the keyboard (decisions 031, 032): which item has focus
// (lit like a hover), Enter or Space clicking it, the system-wide shortcuts,
// and recording a shortcut's new keys for Settings. The rules are core's
// (core/keyboard, core/shortcuts); this keeps the state and makes the calls.

#include "core/hit_test.h"
#include "core/island.h"
#include "core/keyboard.h"
#include "core/layout.h"
#include "platform/win.h"

#include <functional>
#include <optional>
#include <string>

namespace cup {
class SettingsProvider;
}

namespace cup::app {

// What it needs from the app, which owns the state and the loop (main.cpp).
struct KeyboardHooks {
    std::function<const IslandState&()> state;
    std::function<const Scene&()> scene;
    std::function<void(const Event&, const char* reason)> dispatch;
    std::function<void(const std::string& id)> clickTab; // "" = Home
    std::function<void(const Tile&)> clickTile;
    std::function<void(const Hit&)> clickElement;        // with the press feedback a click gives
    HWND window{};                                       // the interaction window: comes to the front with a shortcut
    SettingsProvider* settings = nullptr;                // records new keys for a shortcut
};

// Set before the message loop runs.
void setKeyboardHooks(KeyboardHooks hooks);

// What has focus, for the hover code (its point) and the focus ring (its
// frame). Nothing: the pointer leads, or no card is open.
std::optional<Focusable> keyFocus();

// The pointer moved (render-layer point). A real move takes the lead back;
// Windows repeats one when windows change under a still pointer.
void pointerMoved(Vec2 canvasPoint);

// Settings is waiting for a shortcut's new keys (SettingsHooks::captureKeys).
void captureKeys(bool on);

// A key pressed while recording: it's the new keys (Esc: none). True if it
// was recording, so nothing else may act on the key.
bool recordKey(WPARAM key);

// The keys that move around the island (decisions 031, 033): the arrows,
// Tab and Shift+Tab, Home/End, Ctrl+Tab and Ctrl+Shift+Tab (the next and
// previous tab), 1 to 9 (Home's widgets), and
// Enter/Space. True if `key` was one.
bool navigationKey(WPARAM key);

// A card that is its field (Content::fieldFirst: search, decision 079): the
// field keeps the keys for typing while Up and Down move through the card's
// rows, as in a launcher. Down from the field goes to the first row; Up from
// the first row back to typing. True if `key` was one of them.
bool fieldCardKey(WPARAM key);
// Enter on such a card: clicks the highlighted row, if one is. False: none
// is, and Enter goes to the field (the top result, for search).
bool activateFocused();
// The field's text changed: what was highlighted may not be there any more.
void dropKeyFocus();

// WM_HOTKEY: runs its command, or while recording, counts as those keys.
void hotkeyPressed(WPARAM id);

} // namespace cup::app
