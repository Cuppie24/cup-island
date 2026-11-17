# 149: A window's own minimum size

Status: Accepted (amends 104). Core tested; resizing on Windows needs a run.

**Context.** Every provider's window could be made as small as
`windowMinSize` (320 × 240). Pomodoro's page is a rail and cards at least
`windowCardMinWidth` (560) wide: squeezed under that, its cards got
narrower than they're designed for. The user asked for a minimum size.

**Decision.** `WindowSpec::minSize`: a provider asks for a bigger minimum
(a tokens() size, so scaled for the display like every length, decision
017), never a smaller one: `windowMinSizeOf` is the larger of the two, both
for the frame Windows lets it be dragged to (WM_GETMINMAXINFO) and for a
saved size coming back. Pomodoro asks for `pomodoroWindowMinSize`, 720 × 520:
its rail, a page's padding either side and one card.
