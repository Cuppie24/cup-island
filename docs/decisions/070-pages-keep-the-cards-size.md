# 070: A card's pages keep its size; room to breathe

Status: Accepted (amends 061, 064)

**Context.** Opening Pomodoro's statistics or settings resized the island
to fit the page; the statistics page narrowed everything, its back arrow
too; both pages were packed tight; and the corner said "0 days" before
there was a run to speak of. Rows sit `rowSpacing` apart, which suits rows
of one list and nothing else: groups need their own gap, and these pages
never got one.

**Decision.**
- **A page keeps its card's size** (`Content::sizedLike`): the page carries
  its card's main rows, and the host sizes the card from those, not from
  the page. The statistics are centred in that height, as any card's
  rows are. The settings start at the top (`Content::pageAtTop`) and
  everything under the title is one list whose window fills the rest of
  the card (`Row::visibleRows < 0`, the slot's last list): it scrolls when
  there's more than fits.
- **`Row::narrow`** replaces `CardLayout::Narrow`: a row at most
  `narrowRowWidth` wide, centred. The statistics' tables and line are
  narrow; the title row keeps the card's width, its back arrow at the
  card's left edge.
- **Room:** each group starts `apart` (`groupGap`): on the statistics,
  "Today", the line, "Personal records"; on the settings, the plan under
  the title, "Days off", "Debug". The rule from 066 holds for every page:
  rows of a group sit `rowSpacing` apart, groups `groupGap`.
- **The corner** says "3 days in a row" (the number bright), and nothing
  while there's no run.

**Consequences.** Tested in `pomodoro_tests.cpp` (the pages' height,
centring, the arrow's place, the gaps) and `pomodoro_stats_tests.cpp` (a
page taller than its card scrolls from the top).
