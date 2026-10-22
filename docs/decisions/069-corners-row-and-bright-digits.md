# 069: The corners' own row, and bright digits in one text

Status: Accepted (amends 065, 067, 068)

**Context.** "Room above the timer" meant: the days in a row, the
statistics and the gear are one top row, and everything, the timer too,
starts under it. 065/067 had put the corners beside the ring's top
(`Row::edgesAtTop`) and the ring a gap lower. And 068's bright values,
separate texts in a row, put an item gap (8 px) after "0" where a space
belongs, and lit "min" with "6".

**Decision.**
- **The corners' row:** the card's first row has only edges: the days in
  a row on the left, the statistics and the gear on the right. The dial is
  the next row. `Row::edgesAtTop`, and its special click zones, are gone:
  a 36 px row's buttons click only in their row.
- **`Text::brightDigits`** replaces `Text::bright`: one text whose runs of
  digits are drawn in `titleTextColor` (a text layout's drawing effect per
  run), the rest in its style's colour. One text, so its spaces are the
  font's, and only numbers light up: **2** cycles · **3** h **20** min,
  🔥 **3** days.
- **`canvasSize` 500×720:** the card with its corners' row, the dial and
  five to-dos.

**Consequences.** Tested in `pomodoro_tests.cpp` and
`pomodoro_stats_tests.cpp`; the colours need a Windows run.
