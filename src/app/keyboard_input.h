#pragma once
// The app's side of the keyboard (decisions 031, 032): which item has focus
// (lit like a hover), Enter or Space clicking it, the system-wide shortcuts,
// and recording a shortcut's new keys for Settings. The rules are core's
// (core/keyboard, core/shortcuts); this keeps the state and makes the calls.

#include "core/hit_test.h"
#include "core/island.h"
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
    std::function<void()> updateHover;
    HWND window{};                                       // the interaction window: comes to the front with a shortcut
    SettingsProvider* settings = nullptr;                // records new keys for a shortcut
};

// Set before the message loop runs.
void setKeyboardHooks(KeyboardHooks hooks);

// Where focus is, for the hover code. Nothing: the pointer leads.
std::optional<Vec2> keyFocusPoint();

// The pointer moved (render-layer point). A real move takes the lead back;
// Windows repeats one when windows change under a still pointer.
void pointerMoved(Vec2 canvasPoint);

// Settings is waiting for a shortcut's new keys (SettingsHooks::captureKeys).
void captureKeys(bool on);

// A key pressed while recording: it's the new keys (Esc: none). True if it
// was recording, so nothing else may act on the key.
bool recordKey(WPARAM key);

// The arrows and Enter/Space on the island. True if `key` was one.
bool navigationKey(WPARAM key);

// WM_HOTKEY: runs its command, or while recording, counts as those keys.
void hotkeyPressed(WPARAM id);

} // namespace cup::app
