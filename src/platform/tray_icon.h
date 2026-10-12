#pragma once
// Notification-area (tray) icon: the way to quit (Esc only closes what's open
// on the island, decision 031).
// Right-click (or Shift+F10) opens a menu with Exit. Start with Windows
// moved to the Settings card (decision 030).

#include "platform/win.h"

#include <functional>

namespace cup::platform {

// Adds the icon (the exe's icon resource 1, see assets/cup_island.rc) and
// re-adds it if Explorer restarts. onExit runs on the UI thread when Exit is picked.
void createTrayIcon(HINSTANCE instance, std::function<void()> onExit);

// Removes the icon. Call before exiting, or a dead icon stays until hovered.
void removeTrayIcon();

} // namespace cup::platform
