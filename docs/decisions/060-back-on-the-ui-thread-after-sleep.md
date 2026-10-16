# 060: The island froze after sleep (coroutines on the wrong thread)

Status: Accepted (fixed; confirmed on Windows after a sleep)

**Symptom.** After the PC slept (or hibernated) and woke, the island was
on screen but frozen: no animation, and clicks did nothing you could see.
Quitting from the tray then hung until the exit watchdog killed it
(`exit stuck for 3 s; terminating`). Only a restart helped. It didn't
happen after every sleep.

**How it was found.** The log. Clicks after the wake still changed the
island's state (`Split -> Expanded (click)` lines kept coming), so input
and the state machine were alive; only the picture wasn't. The bracketed
number on each line is the thread, and every line normally carries the UI
thread's (11928 in that log). Right after the wake:

    19:39:00.474 [24240] media: read failed (0x80010100), retrying
    19:39:00.474 [24240] media: new track
    19:39:00.475 [24240] Split -> Alert (media) For the First Time - Mac DeMarco

A pool thread had read the media session, changed the island's state and
rebuilt its visuals. Earlier the same day, a smaller case:
`[18924] read failed (0x80010105)` then `[18924] media: paused`.

**Cause.** A C++/WinRT `co_await` on a WinRT call resumes on the caller's
apartment, here the UI thread. When it can't switch back, it doesn't: it
throws (`RPC_E_SYS_CALL_FAILED` 0x80010100, `RPC_E_SERVERFAULT`
0x80010105) on whatever pool thread completed the call. Right after a wake
that switch fails. The media read caught the error and carried on from
the pool thread: logged, scheduled a retry, started the next read (the
one-at-a-time loop of 057), and that read reached the presenter, the host
and `dispatch`. So the island's state and its Composition visuals were
changed from a thread that doesn't own them. Composition objects belong
to the thread that created them, so they were left broken: frozen until a
restart, and stuck at exit.

**Fix.**
- The media provider never trusts where a `co_await` resumes. After each
  WinRT call it notes any error, then `co_await onUi()`: if not on the UI
  thread, the rest of the coroutine is posted through post_ (our
  DispatcherQueue), which works whatever state the apartment is in.
  Errors are handled only after that. The cover decode's result and the
  first connection to Windows go back the same way.
- Safety net in `dispatch`: called from any thread but the UI thread, it
  logs `dispatch (…) came from a background thread: moved to the UI
  thread` and re-queues itself there, instead of touching state or
  Composition.
- The log notes `going to sleep` / `woke from sleep` (`WM_POWERBROADCAST`).

**How to check it now.** Play music, sleep the PC, wake it: the island
animates and reacts. In the log, the lines after `woke from sleep` carry
the same thread number as the rest. A `came from a background thread`
line means some code went off the UI thread and the net caught it: that
code needs the same fix.

**Lessons.**
- After a `co_await`, check which thread you're on; don't assume it. A
  failed apartment switch throws on the pool thread, and a `catch` runs
  there too.
- Code that owns UI state should have one door: return through your own
  queue (it never fails to switch), and guard the entry point
  (`dispatch`) so a mistake is logged and fixed up, not silently harmful.
- Put the thread id in every log line. Here it was the whole diagnosis.
- Sleep and resume are their own test case: COM, RPC and devices come back
  in odd states.
