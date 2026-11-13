# 106: Pomodoro's card is a timer; the rest is its window

Status: Accepted (amends 061, 063 to 070 and 085: what was on the card and its pages is in the window). Core and provider tested; the window on Windows needs a run.

**Context.** Pomodoro's card held the most of any card: corners with the
days in a row, the statistics and the gear; the dial; the phase dots and
what they mean; the controls; today's line; the to-do list with its field;
and two pages behind the corners (decisions 061 to 070). It needed a wider
island (decision 085) and still read cramped. Decision 104 gave providers
windows and named "Pomodoro's full view" as next; decision 105 let windows
take typing, which the to-dos need.

**Decision.**
- **The card is a timer, the media card's size.** Laid out like media's
  (decision 094), so the two are one size by the same rule
  (`standardCardHeight`: 480 × 203 with the current tokens): the ring where
  the cover goes, the time left big beside it (`TextStyle::Display`), the
  phase and round under it ("Focus · round 1 of 4"), a bar with the time gone
  and left, then the controls, ▶/⏸ the filled circle between stop and skip,
  as media's play sits between previous and next. Dormant: the planned time
  in the empty ring and ▶. No `wideCard`; no dial, dots, today, to-dos or
  pages on the island.
- **An expand button** (`Glyph::Expand`) in the card's top-right corner,
  where media's app switch goes, opens the window (`kOpenWindow`), or brings
  it forward, as Settings' widget does (decision 104).
- **The window** (`providers/pomodoro/pomodoro_window`, pure), "Focus
  timer", Settings' size class, one size. Pages in its sidebar:
  - *Timer*: the dial (decision 065), the phase dots and "2 of 8 done", the
    controls (▶ while dormant, with the plan in words); Today (cycles, focus
    time, days in a row, a fire from three on); To-dos (decision 016): the
    field (typed into as decision 105 says), the list, the Done row, the clear buttons.
  - *Statistics*: today and the personal records as tables (decision 064),
    "Record" tags, Reset statistics (decision 071).
  - *Settings*: the plan's − and + (dimmed while a session runs, and the
    group's label says why), the days off, the Debug row in Debug builds.
- **Questions in place.** A window has no dialogs over it, so the stop
  button, Reset statistics, Clear solved and Clear all ask in the window,
  where they were clicked: the question and Cancel / the confirm button take
  the controls' (or the clear buttons') place. Picking another page or
  closing the window leaves it unanswered. A to-do deleted in the window gets
  an Undo row over the list instead of a toast; the next change takes it away.
  On the card, stop still asks with the island's dialog, and Ctrl+W too
  (decisions 020, 034).
- **Lists in the window don't drag or scroll by themselves** (decision 104):
  to-dos are reordered on no surface for now. Editing a to-do in place works
  (decision 105).

**Not taken.** Keeping today's line or the to-do field on the card: the
user asked for the timer and its controls only. A dial on the card: at its
size it alone is taller than media's whole card. Opening the window by
itself when a session starts: windows open only on the user's click
(decision 104).

**Consequences.** `Content::wideCard` has no user, kept for a card that
needs room. The to-do card tests (lists, dragging, toasts, dialogs on a card)
run on a fixture card (`tests/todo_card_fixture.h`), as Settings' old card
survives for keyboard tests. Reordering to-dos needs list dragging in
windows first.
