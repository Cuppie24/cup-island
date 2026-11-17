# 144: Media sessions are read on a thread of their own

Status: Accepted (follows 060 and 116). Windows-only code: needs a build and a sleep on Windows.

**Symptom.** After the PC slept and woke, the island stopped reacting to
clicks, or seemed to crash. It was in fact frozen, and the user ended it from
Task Manager ("no crash record: it was ended from outside" at the next start).

**How it was found.** The frozen run's log ends here:

    13:08:46.320 [4504] media: paused (from the playback event)
    13:11:24.814 [4504] media: playback changed
    (nothing until the restart at 13:11:31)

`playback changed` is the first event after the wake (Spotify's), on the UI
thread (4504), and it is always followed by more lines: the next state, a
`Split -> Expanded (click)` when the island is clicked. Here nothing came, so
the UI thread never got back from handling it.

Two more things were missing from that log:
- No `going to sleep` / `woke from sleep` in five days of logs. That PC uses
  Modern Standby, which doesn't broadcast `WM_POWERBROADCAST` to windows, so
  the sleep wasn't visible at all.
- No `HANG:`. The watchdog *sent* `WM_NULL`, and a thread waiting on a call
  into another process still handles sent messages. The thread answered the
  watchdog while clicks, which are posted, went unread.

**Cause.** Every call to a media session or the session manager
(`GetPlaybackInfo`, `GetTimelineProperties`, `SourceAppUserModelId`,
`GetSessions`, `GetCurrentSession`, subscribing to an event) goes to another
process, and all of them ran on the UI thread. Usually they take
microseconds. After a wake the other side (Spotify, Windows' media service)
can stall, and a call to it has no timeout. Decision 116 made one such call
synchronous on every playback event, `readPlayback`, which made the window
easy to hit, but `pickSession` and the full read made the same kind of call.

**Decision.**
- **`SmtcReader` owns everything that talks to Windows' media sessions**, on a
  thread of its own (`DispatcherQueueController::CreateOnDedicatedThread`,
  Windows 10 1709+). Its logic is what the provider did before. Events,
  timers and cover decodes come back to it through its own queue (060's
  rule, now pointed at this thread). It reports through callbacks that the
  provider posts to the UI thread, where the presenter and the Host live.
- **The provider is the UI thread's side**: the presenter, and opening the
  app when the art is clicked. That must stay on the click's thread (it
  hands over the foreground), and it uses the last full read's app and title.
- **A hung call stops only the media updating.** `stop()` doesn't wait: the
  reader holds itself through its queued work. If a call never returns, its
  thread stays parked and the island goes on.
- **The reader copies the tokens at start.** `tokens()` belongs to the UI
  thread, and a reload rewrites it. The clock is the Host's, anchored once.
- **The watchdog posts its ping** (a registered message) and counts
  answers. A posted message is handled only once the UI thread is back in
  its own message loop, the same point where clicks get handled.
- **Sleep shows in the log on Modern Standby too.** The interaction window
  registers for suspend/resume and for the screen's state (`screen off` /
  `screen on` / `screen dimmed`), Windows 8+.
- **Copy logs takes the end of `cup-island.old.log` when the log is short.**
  The log is renamed at a start once it's over 2 MB, and the start after a
  freeze is a likely one: the freeze was all in the old file.

**How to check it.** Play music, sleep the PC, wake it, and click the island
at once. It reacts. The log shows `screen off`, then `screen on` (or
`going to sleep` / `woke from sleep`), and the media lines after the wake
carry the reader's thread number, not the UI thread's. A `HANG:` line now
means the UI thread really stopped reading its messages: its dump shows where.

**Lessons.**
- A call into another process is I/O, however synchronous and cheap it
  looks. Keep it off the thread that draws and reads clicks.
- A watchdog has to ask the same way input arrives (posted), or a thread
  stuck in a COM wait looks alive.
