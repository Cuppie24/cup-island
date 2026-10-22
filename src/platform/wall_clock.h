#pragma once
// The wall clock, for core (which reads no clocks, decision 009): the local
// time of day, and whether the user's locale uses a 24-hour clock ("H" in
// its time format).

#include "core/island.h"

namespace cup::platform {

ClockSynced readWallClock();

// Local time minus UTC right now, daylight saving included (Host::utcOffset).
Ms utcOffset();

} // namespace cup::platform
