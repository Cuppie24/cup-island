# 014: Who decides what a card looks like

Status: Accepted

**Context.** Pomodoro's card needed a different shape than media's: the
controls on top, a big countdown in the middle with its ring, the phase
dots centred and bigger, no linear bar. The Expanded card had one fixed
arrangement (art square left, title, corner button, bottom rows), and
nothing said which of these choices belong to a provider and which to the
host.

**Decision.** Providers decide *what* and *in which order*; the host
decides *how it looks*.
- **Providers choose:** which items go in which slot, the order of rows,
  each row's alignment, and which of the host's presets and styles to use
  (a card layout, a text style, a ring vs. a bar). They never give sizes,
  positions, fonts or colours in pixels or values.
- **The host owns:** every size, gap, font size, weight and colour (all in
  tokens.json), the slot geometry of each preset, and how items are drawn.
  So every provider's card looks like the same island.

What this added:
- **Card presets** (`Content::card`): `Standard` (media: art square, title,
  corner button, bottom rows) and `Centred` (only `expandedCenter`, full
  width, rows stacked and centred: a control panel, a timer). New presets
  come when a provider needs one, never per-provider geometry.
- **`TextStyle::Display`:** a big number for the card's centre
  (`displayFontSize`, light weight). A provider asks for "the big style";
  the host decides how big.
- **Rings size with their row:** next to other items a ring is as tall as
  the row (a ring beside a big countdown); alone it fills the slot's square
  as before (compact, the bubble).
- **Large time labels are as wide as their widest digits** ("0:00" measured
  with the real font), so a centred countdown doesn't jump as it ticks and
  sits snug next to its ring. Digits-style labels keep their fixed column,
  so media's time labels still line up.
- **`Row::span`:** a row can span exactly its neighbour's content (the row
  above or below, `MatchAbove` / `MatchBelow`) instead of the slot, so a
  bar is as wide as the dots under it. A relationship the provider states;
  the host works out the width (two passes: natural spans, then the rows
  that match).
- **Steps are bigger** (`stepDotSize`, `stepDotGap`): a host size, the same
  for every provider using them.

**Consequences.** Pomodoro's card: pause/skip/stop on top, a Display
countdown centred in the middle, the phase's bar under it (as wide as the
dots), the phase dots under that, then "Focus · 3 of 8 done". No ring in the card (one beside the
digits was tried and dropped as clutter); the ring stays in compact and the
bubble, where there's no room for digits. A bar that isn't seekable is
only as tall as its line: it needs no click area, and the card has no
room to spare. Tested in `tests/item_tests.cpp` and `tests/pomodoro_tests.cpp`.
