# 156: The timer card's expand button says it's new

Status: Accepted.

**Context.** The timer card's expand button opens the timer's window
(decisions 106, 117): tasks in groups, statistics, settings. It's a small
secondary button in the card's header, and people who knew the timer
before the window came may never press it. The user asked for it to be
marked new in the next update.

**Decision.**
- **A dot on the button** (`Button::badge`, decision 038), the same red dot
  as Settings' rows with something new behind them: "something is behind
  this". Not a "New" tag (decision 048): the button is a bare glyph in a
  full header, with no text to put a tag after.
- **Until the window is first opened.** Pomodoro keeps it in its store
  (`windowseen`, "1" once opened) and drops the dot from the card at once.
  Nothing saved means not opened, so everyone sees it once after the
  update, including people who opened the window before. Nothing recorded
  that, and a dot that goes on the first press costs them one click.
- **Only the button.** No dot on the widget or the tab: the card is where
  the button is, and decision 110's "New" on the widget already covers
  getting people to open the card. Raising the panel's `announce` would
  bring that back too, if the button alone isn't enough.

(Not chosen: a host-wide "new until pressed" for any button, like 110 did
for widgets. With one button asking for it, that would be machinery for
one case. If a second one comes, move this to the host then.)

**Consequences.** `pomodoroPanel` and `pomodoroContent` take `windowNew`.
`tests/pomodoro_tests.cpp` checks the dot before the window is opened,
its absence after, and after a restart. No core change: `badgeCentres`
already places a card button's dot.
