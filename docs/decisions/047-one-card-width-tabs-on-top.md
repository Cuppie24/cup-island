# 047: One card width; the tab row on top

Status: Accepted (amends 009 and 043)

**Context.** Pomodoro's card was 440 wide (decision 043), every other card
and Home 380: switching between them changed the island's width for no
reason. And the tab row ran along the bottom of the card, far from where
the island hangs from the screen's edge, where the eye meets it first.

**Decision.**
- **Every card and Home is `expandedSize` wide: 440** (from 380); the
  separate `centredCardWidth` is gone. Settings' rows (`listCardWidth`)
  widen with it, 340 → 400, so they still fill the card.
- **The tab row runs along the top**, the card's padding above it, and the
  content under it: laid out as before, as if from the card's top, then
  moved down by the strip's space (`tabStripSpace`), so a card's content
  keeps its own layout with or without tabs. Home's button and the pin
  (decision 046) reach into the card's top corners; every tab's click area
  reaches the card's top edge and half the gap under the row.
- **Keyboard:** Tab still starts in the card (its first row first) and
  reaches the tab row last; Up from the card's top row reaches the tabs.

**Consequences.** `layout.cpp` moves the content; `tabs.cpp` places the
strip. Tests in content_tests, home_tests, todo_tests, pomodoro_tests and
keyboard_tests.
