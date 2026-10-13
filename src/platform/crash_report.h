#pragma once
// Crashes that kill the process before it can say anything (decision 013):
// a fail-fast (a WinRT exception escaping a callback, heap corruption)
// skips every handler in watchdog.h. Windows still records them in its
// Application event log, so the next start notices the previous run never
// exited and copies that record into our log.

namespace cup::platform {

// Call once at start, after startLog(). If the previous run didn't exit
// cleanly, logs so, plus Windows' crash records since it started (on a
// background thread). Then marks this run as running.
void reportPreviousCrash();

// Call on a deliberate exit, so the next start doesn't report one.
void markCleanExit();

} // namespace cup::platform
