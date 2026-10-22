# 044: Setting focus and break lengths before starting

Status: Accepted; − and + placed and shown on hover by 063

**Context.** Focus and break lengths were tokens.json values: changing them
meant editing a file. And a click on the Pomodoro card's empty space closed
it, though the card is a page of controls and to-dos.

**Decision.**
- **The dormant card sets the plan:** under the big focus time, a Focus row
  and a Break row, each its name, its length ("25 min") and a full-size −
  and + at the end (`buttonSize`: easy to hit), dimmed at a limit. They
  replace the "25 min focus, 5 min breaks" caption. Picked from four
  sketches (steppers, two big numbers, presets, typed numbers): the most
  obvious one.
- **Steps and limits are tokens:** `pomodoroFocusStepMs` (5 min) up to
  `pomodoroFocusMaxMs` (2 h), `pomodoroBreakStepMs` (1 min) up to
  `pomodoroBreakMaxMs` (1 h); never below one step. `pomodoroFocusMs` and
  `pomodoroBreakMs` are now the defaults.
- **Kept** in the store under `plan` ("plan1 <focus ms> <break ms>",
  decision 028), out-of-range values clamped. A session's phases, and its
  catch-up after a restart (027), take their lengths from the plan. The long
  break and the rounds stay in tokens.json.
- **Only while dormant:** a running session's card has no − / +, so the
  plan can't change under a session.
- **The card stays open** on a click between its controls
  (`Content::keepOpen`, as Settings); Esc, a click outside the island and
  the idle timeout still close it.
- **Room:** the canvas grows to 500×640, so the dormant card with its rows,
  the to-do list and tabs fits with its shadow.

**Consequences.** `pomodoro_session` (`PomodoroPlan`, `adjustPlan`,
`loadPlan`) and the card are tested in `tests/pomodoro_tests.cpp`. No wheel
or keyboard shortcut for the lengths yet: − and + only (Tab reaches them).
