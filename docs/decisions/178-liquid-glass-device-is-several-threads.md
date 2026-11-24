# 178: Liquid glass's Direct3D device is locked for several threads (the island vanished mid-video)

Status: Accepted; built, not yet run on Windows. The cause is the likeliest
one read from the log and the code, not a confirmed one (amends 165, 158)

**Symptom.** 0.13.5, liquid glass picked that morning. For 27 minutes with
nothing playing, cards opened and closed without trouble. Then a video
started in the browser, and the island vanished three times in five
minutes, each time a moment after a play/pause. Nothing in the log: no
`CRASH:`, no `HANG:`, no dump, and at the next start `Windows has no crash
record for it`.

**What that rules out.** An ordinary crash, an uncaught C++ exception and
`abort()` are logged (decision 013); a frozen UI thread is logged after
5 s (decision 144). What ends a process past all of those is a fail-fast:
Windows finding the heap corrupted, or an exception leaving a queued task
(decision 158).

**Likeliest cause.** Liquid glass has a Direct3D device of its own, and
three parties use it: the UI thread, the frames' thread (while a shape
moves), and Windows' screen capture, which was started on that device and
works on it whenever the desktop's picture changes. `lock_` keeps the
first two apart. Nothing kept the capture apart from the frames' thread:
`take()` asks the pool for the next picture and closes the old ones
outside the lock, and whatever the capture does inside Windows takes no
lock of ours. A Direct3D context is one thread's unless it is told
otherwise. With a still desktop there are hardly any pictures; with a
video there is one every frame, all through every morph. Memory
overwritten that way is found later, by some unrelated allocation, which
fits the island going a moment after the last thing in the log rather
than at it.

**Decision.**
- **The device is told** (`ID3D11Multithread::SetMultithreadProtected`,
  in `build()`): Direct3D then locks its context for each call, whoever
  makes it. `lock_` stays: it also keeps a frame's passes together.
- **Nothing is thrown back at Windows** from liquid glass's queued tasks
  (`onReady`, `lost`: through `guardedTask`) or from `take()` (which now
  catches `std::exception` too): decision 158's rule, not yet applied here.
- **The next one says what it was.**
  - A corrupted heap is logged from a vectored handler
    (`CRASH: Windows found the heap corrupted`), which Windows calls before
    it ends the process; the unhandled-exception filter is never reached.
    No dump: writing one needs the heap.
  - `crash_report.cpp` read Windows' record with `EvtFormatMessage` and no
    provider metadata, documented to work only for forwarded events; no
    log has ever shown a `Windows recorded:` line. It now falls back to the
    record's own fields (`EvtRender`, XML): the module, the exception code
    and the offset.

**If it happens again.** The log will have one of: `CRASH: Windows found
the heap corrupted` (memory, as guessed: look at what else shares a
thread's data), or `Windows recorded: <EventData>...` with a module and a
code (0xc000027b: an exception left a queued task or an event handler;
0xc0000409: a fail-fast). Either narrows it to one kind.

**Rule.** A Direct3D device handed to something else in Windows (capture,
video decoding) is used from threads that aren't ours: turn its
multithread protection on when it's made.
