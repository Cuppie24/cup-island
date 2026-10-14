# 050: A shortcut to open the activity's card

Status: Accepted

**Context.** Open Home (decision 032) reaches Home from any app, but the
thing usually wanted is the card of what's running: the song, the timer.
That took Ctrl+Q, then a click or the arrows to its tab.

**Decision.**
- **A second system-wide command, Open the activity, Ctrl+Alt+Q by
  default** (next to Open Home's Ctrl+Q, so the pair is easy to remember).
  It's registered, rebindable, saved (`activity=…`) and let go under a
  fullscreen app exactly like Open Home: everything in decision 032 applies.
- **It opens the card of the pill's activity**: the top-ranked one, the
  same card a click on the pill opens (the user's pick, if they made one,
  decision 007). From Idle, Compact, Split, an alert, Home or another card
  alike. In Split it's the pill's, not the bubble's.
- **Pressed again on that card, it closes**, like Open Home's second press.
- **With no activity running it does nothing.** Home already has its own
  shortcut, and a dormant panel isn't "the activity".
- A core event (`ActivityOpened`), so the rule is in `reduce()` and in
  `tests/scenarios/activity_shortcut.scenario`; the app only dispatches it
  and brings the island to the front, as for Open Home.

**Consequences.** Shortcuts saved before this have no `activity=` line and
get the default. If someone had already moved Open Home to Ctrl+Alt+Q, the
loaded line clashes with the new default and is refused (decision 032's
rule: a line that can't be bound keeps the default), so Open Home is back
on Ctrl+Q; they can rebind it in Settings. Two commands now clash for real,
so that rule is tested in `tests/keyboard_tests.cpp`.
