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
- **Shown** whenever no session runs, under ▶, as **one centred line**:
  "2 today best 4 · 🔥 3 days best 5 · 3 h 20 min". Three rows with the
  bests pushed to the far edge looked scattered, and the fire pulled its
  row's text out of line with the others. Each best is in the same style as
  its value, not a dimmer caption. When now beats every earlier day (or
  run), a **"Record"** tag takes the best's place. From 3 days in a row a
  🔥 goes just before the number: Segoe UI Emoji's, in colour, since the
  icon fonts have no fire. "Today" is a plain count: nothing breaks it.
  The line is a `Row::hug` row: texts as wide as they are, the whole
  centred; too wide for the card, it's laid out like any row.
- **Days in a row:** days with a finished session, up to today. Today
  without one yet doesn't break it. **Days off**, picked by the user,
  don't break it either; one worked still counts.
- **Bests are worked out from the days kept**, not stored: the best day
  leaving today out, the longest run that ended before the current one.
  So a record is "now is more than ever before", and changing days off
  re-reads the history with the new days.
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
  Settings page: a back arrow (Esc too), "Days off", then one row of seven
  circles, "Mo" to "Su": a click takes a day off or back, a lit circle is
  off (a Switch's colours). One row, not seven: the week reads at a glance
  and the page stays short. They're `Button::round`, a labelled circle
  any provider can use. Closing the card leaves the page.
- **Debug builds** (the demo keys' switch, `NDEBUG`), or any build started
  under a debugger (`IsDebuggerPresent`: Visual Studio's preset dropdown
  starts on Release), add a "Fake
  statistics" row to that page: each click shows a made-up history (3 in a
  row with best 5; then new records), then the real one again. A made-up
  history is never saved.
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
