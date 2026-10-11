#pragma once
// Keeps the island above other windows (TR §9). WS_EX_TOPMOST alone isn't
// enough: another topmost window raised later (PowerToys "Always on Top",
// overlays, some apps' own windows) covers it, and nothing raised it back,
// so it stayed behind until a restart. Event-driven: re-raised whenever the
// foreground window changes, so idle costs nothing.

#include "platform/win.h"

#include <vector>

namespace cup::platform {

// windows: bottom to top (the render layer, then the interaction layer).
// Logs what was covering the island, the first time each culprit shows up.
void stayOnTop(std::vector<HWND> windows);

} // namespace cup::platform
