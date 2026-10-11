#pragma once
// The alert queue (PS §6.3, §6.4): admission, coalescing, pre-emption and
// replay. Part of reduce(); split out of island.cpp to keep it readable.

#include "core/island.h"

namespace cup {

// Admits, coalesces, pre-empts or queues an alert. True when it's a critical
// one that got through: the caller closes Expanded (only critical interrupts it).
bool postAlert(IslandState& state, Alert alert, Ms now);

// An overlay is taking over: put the showing alert back to replay later, or
// drop it if the island is hidden and the alert isn't urgent (PS §6.4).
void suspendShowingAlert(IslandState& state);

// After every event: end the showing alert on time, drop queued ones held too
// long, and show the next one that's allowed now.
void settleAlerts(IslandState& state, Ms now);

} // namespace cup
