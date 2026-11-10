#pragma once
// Double-tap shortcuts ("Double Ctrl", decision 079), which RegisterHotKey
// can't express: a low-level keyboard hook (WH_KEYBOARD_LL) on a thread of
// its own feeds core's DoubleTap and, when it fires, posts the command to
// the window as an ordinary WM_HOTKEY, so the rest of the app can't tell
// the two kinds of shortcut apart.
//
// Its own thread: Windows drops a low-level hook whose callback is slow
// (LowLevelHooksTimeout), and the UI thread may be busy drawing. The hook
// only watches: every key still goes on to whatever app has focus.
// SetWindowsHookEx(WH_KEYBOARD_LL) works from Windows 2000 on (decision 015).

#include "platform/win.h"

namespace cup::platform {

// Watches for `key` (kCtrl, kShift or kAlt) tapped twice and posts
// WM_HOTKEY(`id`) to `hwnd`. One tap shortcut at a time: a second start
// replaces the first. False if the hook couldn't be set.
bool startTapHook(HWND hwnd, int id, int key);
void stopTapHook(); // when the shortcuts go off (a fullscreen app, decision 032) or the window goes

} // namespace cup::platform
