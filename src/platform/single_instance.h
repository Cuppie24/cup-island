#pragma once
// Only one Cup Island per Windows session (decision 021): a second launch
// asks the running island to show itself, then exits.

#include "platform/win.h"

namespace cup::platform {

// Call first thing in main(), before the log is opened. true: this process
// is the island. false: one is already running; it has been asked to show
// itself, and this process should exit without touching anything shared.
bool becomeTheOnlyInstance();

// The message the running island's interaction window receives when another
// launch asks it to show itself.
UINT showYourselfMessage();

} // namespace cup::platform
