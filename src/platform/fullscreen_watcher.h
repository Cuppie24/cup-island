#pragma once
// Notices when a fullscreen app (borderless or exclusive) covers the island's
// monitor, so the island can hide (TR §9.2). Event-driven: WinEvent hooks on
// foreground changes and on the foreground app's moves/resizes, no polling.

#include "platform/win.h"

#include <functional>

namespace cup::platform {

// `island` is any window on the island's monitor (the render layer).
// `onChange(true)` = hide, `onChange(false)` = show again; called on this
// (the UI) thread, and once right away with the current state.
void watchFullscreen(HWND island, std::function<void(bool fullscreen)> onChange);

} // namespace cup::platform
