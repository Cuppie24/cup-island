#pragma once
// Should the island hide because a fullscreen app covers its monitor (TR §9.2)?
// Pure: the platform gathers window facts, this decides.

#include "core/geometry.h"

#include <optional>

namespace cup {

struct ForegroundWindow {
    Rect bounds;            // GetWindowRect
    Rect monitor;           // rcMonitor of the monitor the window is on
    bool onIslandMonitor = false;
    bool ignorable = false; // desktop, taskbar, Alt-Tab/Task View, the island itself, cloaked windows
    bool minimized = false;
    bool maximizedWithTitleBar = false; // a normal maximized window, never "fullscreen"
};

// `exclusiveFullscreen`: Windows reports a D3D exclusive-fullscreen app running.
// It counts only with a real window in front on the island's monitor (decision 059).
bool shouldHideForFullscreen(const std::optional<ForegroundWindow>& window, bool exclusiveFullscreen);

} // namespace cup
