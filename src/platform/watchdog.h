#pragma once
// Catches what the log alone can't explain (decision 013):
// - a frozen UI thread: checked every few seconds; when it stops answering,
//   the log says so and a minidump of every thread's stack is written next
//   to the log, showing exactly where it's stuck;
// - a crash, an uncaught C++ exception, abort(), a pure virtual call or a
//   C runtime invalid parameter: logged, with a minidump. (A fail-fast
//   can't be caught in-process: crash_report.h picks it up next start.)
// Dumps open in Visual Studio with the matching CupIsland.pdb.

#include "platform/win.h"

namespace cup::platform {

// Installs the crash handlers. Call once, on the UI thread, right after startLog().
void watchForCrashes();

// Starts watching the UI thread through one of its windows. Stops by itself
// once the window is destroyed.
void watchForHangs(HWND uiWindow);

} // namespace cup::platform
