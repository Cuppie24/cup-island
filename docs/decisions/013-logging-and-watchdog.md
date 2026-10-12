# 013: A log file, hang and crash dumps, and a clean exit

Status: Accepted

**Context.** Release builds have no console (decision 011), so problems
reported from real use (the island freezing, Exit leaving the process
running) left no trace. A freeze can't be explained from a log line alone:
it needs the frozen thread's stack.

**Decision.**
- `platform/log.h`: every message goes to
  `%LOCALAPPDATA%\CupIsland\logs\cup-island.log` (one 2 MB generation
  kept), flushed per line, and to the console when there is one. The
  Settings card's "Copy logs" button (`providers/system/settings_provider.h`,
  pure; a Home tile of its own until decision 022) puts its last
  64 KB on the clipboard. The Windows-only provider files log through it
  too: they're platform code in all but folder.
- `platform/watchdog.h`: every 5 s a background thread sends the UI thread
  `WM_NULL` and waits 5 s for an answer. No answer: the log says HANG and a
  minidump of every thread goes next to the log, once per hang. Crashes and
  `std::terminate` (an exception escaping a `fire_and_forget` coroutine)
  log and dump too. This is the island's only periodic wake-up besides the
  idle clock's: a cheap message every 5 s, never a data source polling (TR §8).
- **Exit.** `main()` used to block on `ShutdownQueueAsync().get()` on the
  UI thread, but that shutdown completes *on* the UI thread: a deadlock, so
  the process never ended. It now keeps pumping messages until the
  shutdown completes, and a 3 s safety net terminates the process if
  anything else in teardown blocks.

**Silent crashes.** Some crashes end the process before any handler above
runs, leaving no log line and no dump. They are covered two ways:
- **More handlers** (`watchdog.cpp`): `abort()` (SIGABRT), pure virtual
  calls and C runtime invalid parameters now log and dump too, and the UI
  thread keeps 64 KB of stack after an overflow
  (`SetThreadStackGuarantee`) so the filter still has room to run. Only the
  first handler reports; the others it trips on the way down stay quiet.
- **Next start** (`crash_report.cpp`): a fail-fast (a WinRT exception
  escaping a callback, `0xC000027B`; heap corruption) can't be caught
  in-process at all. So `running.txt` in the log folder marks a running
  island, and a deliberate exit deletes it. If it's still there at start,
  the log says `PREVIOUS RUN ENDED UNEXPECTEDLY` and copies Windows' own
  Application event log records for CupIsland.exe since that run started
  (Application Error 1000: exception code, faulting module and offset;
  Application Hang 1002). It reads the event log with `wevtapi`, on a
  background thread, once. No record means it was ended from outside
  (Task Manager, sign-out, power loss).

**Consequences.** Freeze reports come with a `.dmp`: open it in Visual
Studio with the build's `CupIsland.pdb` (Debug → Windows → Parallel Stacks
shows the UI thread's stack). Dumps are only useful with the pdb of the
exact build that wrote them, so a release should keep its pdb.
