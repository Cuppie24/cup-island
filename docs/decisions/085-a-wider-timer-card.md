# 085: The timer's card is wider

Status: Accepted (amends 047)

**Context.** Every card is `expandedSize` wide (decision 047), so the
island keeps its width when tabs switch. Pomodoro's card holds the most:
the dial, the phase dots, the controls and the to-dos under them, with
its statistics and settings pages; at that width its to-dos wrap early
and it reads cramped. While its card floated (decision 081) it could be
resized; back on the island, it needs the room there.

**Decision.**
- **A card can ask for more room** (`Content::wideCard`): it's
  `wideCardWidth` wide (640, as wide as the search card), its pages too,
  since a page keeps its card's size (decision 070). The provider says it
  wants room; the host says how much (decision 014).
- **Only the timer asks**: Pomodoro's dormant and running cards. Home and
  every other card stay `expandedSize` wide.
- It grows to both sides from the island's centre, as every card does.

**Consequences.** Switching tabs between the timer and another card now
changes the island's width, which decision 047 avoided; the morph carries
it (decision 080). If that reads as a jump, the next step is the timer's
own tab row keeping the wide width. `layout.cpp` picks the width next to
the search card's; tests in todo_tests and pomodoro_tests.
