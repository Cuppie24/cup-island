#pragma once
// The connected monitors, for putting the island on the one the user dragged
// it to (decisions 023, 178).

#include "platform/win.h"

#include <optional>
#include <string>
#include <vector>

namespace cup::platform {

struct Display {
    std::string id;     // stable across reboots and replugging: the monitor's device path
    std::string name;   // its friendly name ("DELL U2720Q"); identical models get " (2)"
    bool primary = false;
    Rect bounds;        // the whole monitor (rcMonitor), in screen pixels
    Rect workArea;      // ...less the taskbar (rcWork): where providers' windows go (decision 104)
    float scale = 1.0f; // its scaling, DPI / 96: 1.5 at 150%
};

// In Windows' order. Empty only if Windows reports none (no display at all).
std::vector<Display> listDisplays();

// The display with this id; for "" or one that isn't connected, the primary.
Display displayFor(const std::string& id);

// The display this point (screen pixels) is on: where a lifted island is
// carried to (decision 178). None between or beyond monitors.
std::optional<Display> displayAt(Vec2 screenPoint);

} // namespace cup::platform
