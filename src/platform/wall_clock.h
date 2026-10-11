#pragma once
// The wall clock, for core (which reads no clocks, decision 009): the local
// time of day, whether the user's locale uses a 24-hour clock ("H" in its
// time format), and today's date in the locale's words ("Sat 11 Oct").

#include "core/island.h"

namespace cup::platform {

ClockSynced readWallClock();

} // namespace cup::platform
