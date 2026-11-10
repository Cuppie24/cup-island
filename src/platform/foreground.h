#pragma once
// Bringing the island to the front with the keys, for a shortcut (decisions
// 079, 089). SetForegroundWindow alone isn't enough: a double tap isn't a
// real hotkey, so Windows gives no leave to come forward, and with Start or
// Windows search open, the flyout keeps the foreground even when the call
// "succeeds". So: check what's really in front, close a shell flyout first
// (Esc, as a person would), and try again for a moment while it goes.
// SetForegroundWindow, AttachThreadInput, SendInput and
// QueryFullProcessImageName are all Windows Vista or older (decision 015).

#include "platform/win.h"

namespace cup::platform {

// Puts `hwnd` (a window of this thread) in front, focused. When it can't at
// once, it keeps trying on a timer, foregroundRetry apart, at most
// foregroundRetries times; a later call replaces an earlier one's tries.
void bringToFront(HWND hwnd);

} // namespace cup::platform
