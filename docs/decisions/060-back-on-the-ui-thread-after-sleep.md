# 060: Coroutines return to the UI thread through our own queue

Status: Accepted

**Context.** After the PC slept, the island showed but didn't react to
clicks: the state changed (the log had `Split -> Expanded (click)`), the
picture didn't, and quitting hung (`exit stuck for 3 s`). The log's thread
ids gave it away. Right after waking:

    [24240] media: read failed (0x80010100), retrying
    [24240] media: new track
    [24240] Split -> Alert (media) …

on a pool thread, not the UI thread (11928). A C++/WinRT `co_await` of a
WinRT call resumes on the caller's apartment (the UI thread), except when
it can't switch back: then (`RPC_E_SYS_CALL_FAILED`, `RPC_E_SERVERFAULT`;
seen after sleep) it throws on the pool thread instead. The media read's
`catch` carried on from there, started the next read and dispatched: the
island's state and its Composition visuals were changed from a thread that
doesn't own them, which left them broken until a restart.

**Decision.**
- The media provider never trusts where a `co_await` resumes. After each
  WinRT call it notes any error and does `co_await onUi()`: if not on the
  UI thread, it posts the rest of the coroutine through post_ (our
  DispatcherQueue), which works whatever state the apartment's in. Errors
  are handled after that. The cover decode's result is posted the same
  way, and so is the first connection's.
- Safety net in `dispatch`: called from any thread but the UI thread, it
  logs `dispatch (…) came from a background thread` and re-queues itself
  there instead of touching state or Composition.
- The log notes `going to sleep` / `woke from sleep`.

**Consequences.** Needs a Windows run with a sleep to see. Any new
coroutine that touches the island after a WinRT call should go back the
same way (or post, as the theme provider does); the dispatch net catches
what's missed and says so in the log.
