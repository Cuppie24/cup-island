#include "core/fullscreen.h"

namespace cup {

bool shouldHideForFullscreen(const std::optional<ForegroundWindow>& window, bool exclusiveFullscreen)
{
    // Exclusive fullscreen bypasses the compositor: the island can't draw over it anyway.
    if (exclusiveFullscreen) return true;
    if (!window || window->ignorable || window->minimized || !window->onIslandMonitor) return false;

    // A maximized window with a title bar can cover the whole monitor when the
    // taskbar auto-hides; that's still a normal window (TR §9.2: maximized
    // Explorer must not hide the island). Borderless fullscreen has no title bar.
    if (window->maximizedWithTitleBar) return false;

    const Rect& w = window->bounds;
    const Rect& m = window->monitor;
    bool coversMonitor = w.x <= m.x && w.y <= m.y && w.x + w.width >= m.x + m.width && w.y + w.height >= m.y + m.height;
    return coversMonitor; // only covering the work area (taskbar visible) is not fullscreen
}

} // namespace cup
