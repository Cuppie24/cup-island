# 111: The card scale is the default; Home's widgets use it

Status: Accepted (amends 019 and 073: a widget's sizes; 107: the timer's stop and skip). Core tested; the look on Windows needs a run.

**Context.** Decision 107 made media's and the timer's cards bigger and
heavier, with more room between their pieces: a 22 px title at 800, the
artist in Body, the large skips and the full ▶ circle, loose controls,
groups apart. Home's widgets were still on the older, denser scale (a 17 px
Title, a 13 px Caption, an 18 px icon in 60 px tiles 8 apart) and read small
beside them. The user asked for the cards' scale to be the default.

**Decision.**
- **`docs/design-rules.md`** writes the scale down: what a card's or
  widget's main line, its second line, its main action and the controls
  beside it use. Lists and windows keep the dense scale.
- **Home's widgets on the card scale.** Their first line is `CardTitle`
  (media's title swims there as on its card), the second `Body`.
  `tileHeight` 74 (from 60), `tileGap` 12 (from 8), `tilePadding` 10 (from
  8): a 29 px CardTitle line, a 20 px Body line and the row spacing fit the
  54 px inside. The leading icon is `tileIconSize` 24 (from 18): Home sets
  `Icon::large` on the leading square's icons; icons in a widget's rows
  stay text-sized, so their row stays as tall as its text.
- **The timer's stop and skip are `Button::large`**, like media's previous
  and next beside the ▶ circle. The row is as tall as the circle either way.

**Not taken.** Raising `titleFontSize` and `buttonSize` themselves: Settings'
list, search's results, menus and windows use them, and those are for
scanning many rows. Promoting a widget's styles inside Home: providers say
which line is which, as on their cards.

**Consequences.** Two rows of widgets (the usual: media and the timer, then
search, settings and the theme) fit the shared 248 card with room to spare.
Three rows need 282, so Home grows when a third row appears (an
update's widget); before, it grew at four.
