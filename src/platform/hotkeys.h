#pragma once
// System-wide shortcuts (decision 032): RegisterHotKey on the interaction
// window, one id per command. Windows posts WM_HOTKEY there whichever app
// is in front, and lets the app that gets it come to the front.

#include "core/shortcuts.h"
#include "platform/win.h"

#include <optional>

namespace cup::platform {

// Registers `chord` for `command`, replacing what it had. False if Windows
// refused (another app has those keys); then the command has none.
bool registerHotkey(HWND hwnd, Command command, const Chord& chord);

void unregisterHotkeys(HWND hwnd); // every command's: before the window goes

// The command a WM_HOTKEY's id stands for.
std::optional<Command> hotkeyCommand(WPARAM id);

// The key pressed (a WM_KEYDOWN's), with the modifiers held now, as a chord a
// shortcut may use; nothing for a modifier on its own or a key shortcuts can't end in.
std::optional<Chord> chordPressed(WPARAM key);

} // namespace cup::platform
