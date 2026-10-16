#pragma once
// Clicks outside the island while it's open but was never activated (decision
// 063). A click elsewhere normally closes it through WM_ACTIVATE; opened by a
// drop (OLE never activates the window under it), it gets none, so it would
// stay open. While that's so, and only then, a low-level mouse hook watches
// for a press outside it and posts outsideClickMessage() to the island.

#include "platform/win.h"

namespace cup::platform {

UINT outsideClickMessage();

// On while `island` is open and not the foreground window; off otherwise. Cheap to call on every change.
void watchOutsideClicks(HWND island, bool on);

} // namespace cup::platform
