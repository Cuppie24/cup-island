# 061: Pomodoro statistics, days off, and coming back paused

Status: Accepted

**Context.** Pomodoro kept nothing about what was done: no "how am I doing
today", no reason to come back tomorrow. And a session kept running while
the app was closed (027), so after a reboot the user found it phases further
on, with time counted that they hadn't worked.

**Decision.**
- **A session** is all the phases: every round of focus with its break,
  then the long break. It **finishes** when the long break ends (or is
  skipped) with at most `pomodoroFocusSkipsAllowed` (1) focus phases
  skipped. More skipped: neither finished nor canceled. Stop (confirmed):
  canceled.
- **Shown** on the dormant card, under ▶: "N finished today · best M" (the
  most in one day), then "D days in a row · 1 h 40 min focused today" once
  there's any. "Finished today" is a plain count: nothing breaks it.
- **Days in a row:** days with a finished session, up to today. Today
  without one yet doesn't break it. **Days off**, picked by the user,
  don't break it either; one worked still counts. The best run is kept.
- **Kept, not shown yet:** per day, sessions started, finished, canceled;
  focus phases done and skipped; breaks skipped; focus time actually spent
  (pauses left out, a skipped or stopped phase's part included); to-dos
  ticked (unticked the same day: taken back).
- **Stored** under `stats` as one line per day with something in it
  (`2025-10-20 6000000 3 2 1 7 1 2 5`, readable), so later views (a week,
  totals) need no new format. Days off under `daysoff` ("daysoff1 0000011").
- **The day starts at local midnight.** Core reads no clocks, so the host
  gives providers `Host::utcOffset()` (Windows: `GetTimeZoneInformation`,
  daylight saving included). The dormant card rebuilds when shown on a new
  day (`onCardShown`), not on a midnight timer.
- **The timer's own settings**, not global Settings: a gear in the card's
  top-right corner (its top row's trailing button) opens a page like a
  Settings page: a back arrow (Esc too), "Days off", a switch per weekday,
  Monday first. Closing the card leaves the page.
- **After a restart a session is back paused**, where it was when the app
  quit (`stop()` saves it). Nothing passes while the app is closed, so
  nothing is counted for time not worked. This replaces 027's catch-up.
  If the app didn't quit cleanly (a crash, Windows ending it at shutdown),
  the session is where it was last saved: its last start, pause or phase
  change.

**Consequences.** `pomodoro_stats` (pure) and `pomodoro_card` (content,
moved out of the provider) are tested in `tests/pomodoro_stats_tests.cpp`.
Session lines are now `pomodoro2` (no wall clock; the skips); 027's
`pomodoro1` lines still load, paused.

**Open.** No view of the kept totals yet. Saving on Windows' shutdown
(`WM_ENDSESSION`) would make a reboot resume exactly where it stopped.
