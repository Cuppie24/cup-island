# 179: No call on a media object Windows didn't give (the island vanished at a play/pause)

Status: Accepted; built, not yet run on Windows (amends 158, 178)

**Symptom.** The island vanished, usually a moment after
`media: playback changed`, with nothing else in the log: 0.12.18 (decision
158), 0.13.5 three times in five minutes (decision 178), 0.13.6 on frosted
glass. 0.13.6 could read Windows' crash record (decision 178), and it said:

    ExceptionCode c0000005, CupIsland.exe, FaultingOffset 0xcd063

With that release's `CupIsland.pdb` (the Release run's artifact) the offset
is in `cup::readPlayback`, 0x333 bytes in.

**Cause.** `readPlayback` called straight into what
`session.GetPlaybackInfo()`, `GetTimelineProperties()` and
`playback.Controls()` returned. Windows can return no object there: a
session between two states, or just closed. In C++/WinRT a call on an
empty object is not an exception. It reads through a null pointer, an
access violation, and no `catch (winrt::hresult_error const&)` sees it.
Which of the three was empty the offset alone doesn't say; all are checked.

So decision 158's diagnosis (an exception leaving a `fire_and_forget`) and
decision 178's (liquid glass's device used from two threads) were guesses
that fitted the same silent logs. Their changes stay: both are right in
themselves. Liquid glass was not the cause.

**Why nothing was logged.** The reader runs on a `DispatcherQueue`'s
thread (decision 144). Windows' queue ends the process itself when a task
on it faults, and the unhandled-exception filter (decision 013) is never
asked. Each log line is flushed as it's written: nothing was lost, nothing
was ever written.

**Decision.**
- **`need()`** (`smtc_reading.cpp`): an empty playback info, controls or
  media properties throws `hresult_error(E_POINTER)`, which every caller
  already treats as a read that failed (shown from the last read; the full
  read retries). An empty timeline reads as an app without one. The
  shuffle, repeat and seek buttons check before they ask.
- **An access violation in CupIsland.exe is logged at first sight**, from
  the vectored handler (decision 178), on any thread, with its offset in
  the exe and a dump: `CRASH: access violation at CupIsland.exe+0x...`.

**Reading an offset.** Download the release run's artifact (the pdbs),
then `llvm-pdbutil dump --section-headers` and `--symbols` on
`CupIsland.pdb`: the offset minus `.text`'s virtual address is the
address in section 1; the `S_GPROC32` whose range holds it is the function.

**Rule.** What a WinRT call returns can be empty even where the docs
promise an object. Before the first call on it: `if (!thing)`. A `try`
does not cover it.
