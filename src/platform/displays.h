#pragma once
// The connected monitors, for putting the island on the one the user picked
// (decision 023).

#include "platform/win.h"

#include <string>
#include <vector>

namespace cup::platform {

struct Display {
    std::string id;     // stable across reboots and replugging: the monitor's device path
    std::string name;   // its friendly name ("DELL U2720Q"); identical models get " (2)"
    bool primary = false;
    Rect bounds;        // the whole monitor (rcMonitor), in screen pixels
    float scale = 1.0f; // its scaling, DPI / 96: 1.5 at 150%
};

// In Windows' order. Empty only if Windows reports none (no display at all).
std::vector<Display> listDisplays();

// The display with this id; for "" or one that isn't connected, the primary.
Display displayFor(const std::string& id);

} // namespace cup::platform
