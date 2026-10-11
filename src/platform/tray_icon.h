#pragma once
// Notification-area (tray) icon. Release builds have no console window, so this
// is the way to quit when the pill doesn't have keyboard focus for Esc.
// Right-click (or Shift+F10) opens a menu: Start with Windows (a checkbox,
// platform/autostart.h) and Exit.

#include "platform/win.h"

#include <functional>

namespace cup::platform {

// Adds the icon (the exe's icon resource 1, see assets/cup_island.rc) and
// re-adds it if Explorer restarts. onExit runs on the UI thread when Exit is picked.
void createTrayIcon(HINSTANCE instance, std::function<void()> onExit);

// Removes the icon. Call before exiting, or a dead icon stays until hovered.
void removeTrayIcon();

} // namespace cup::platform
