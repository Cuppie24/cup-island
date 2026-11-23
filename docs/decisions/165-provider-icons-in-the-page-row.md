# 165: The page row is a constant list of providers' icons

Status: Accepted (replaces 109's dots and 118's arrows; amends 113: where a
panel's button sits; 115: Ctrl+Tab; 019: Search and the theme switch were
widgets). Core tested; the look on Windows needs a run.

**Context.** The page row showed a dot per running activity with ‹ and ›
beside them (decisions 109, 118). Nothing said which activity a dot was,
and a provider with nothing running had no place in the row at all: its
card was reached only through Home. The user asked for a list of the
providers' icons instead, always there, with Search by Home's button and
the dark/light switch by Settings'.

**Decision.**
- **An icon per page, centred in the row**, `tabIconSize` (32) square,
  `tabIconGap` (4) apart: every panel with a card in publish order,
  whether it's running or not, then activities that aren't panels in start
  order. The list changes only when a provider comes or goes, so an icon
  keeps its place. Toggles and static widgets have no card and no icon.
- **The shown page's icon is lit** (`tabSelectedColor` behind it, gliding
  to a newly picked one) and in colour; the others are grey
  (`Placed::muted`, decision 072) until the pointer is on them. On Home
  none is lit: Home's button is.
- **A click shows that page's card.** `ActivitySwitched` on a dormant
  panel opens its card, as its widget does; a live one becomes the pick as
  before (decision 007).
- **`Panel::icon`** is the glyph (media: Music, Pomodoro: Timer); without
  one, and for an activity with no panel, a plain one (`Glyph::Source`).
- **`Panel::rowSpot`** says where a `rowButton` sits: `Corner` (Settings,
  as before), `BesideCorner` (the theme switch, left of Settings whatever
  the publish order) or `BesideHome` (Search, right of Home's button). A
  row button with no tap action opens its card. Search and the theme
  switch are no longer Home widgets.
- **No dots, no arrows.** Ctrl+Tab steps through the list, dormant cards
  included, round; from Home, the first.
- **Too many icons** to fit between the ends' buttons are squeezed
  together, still centred.
- A badge sits in an icon's corner, like a button's; a flagged widget has
  it on its tile and on its icon.

**Not taken.** Each provider's live art (a cover, a ring) as its icon: the
list should look the same whatever is playing. Icons only for running
providers: that's the row that was replaced.

**Consequences.** `tabDotSize`, `tabDotGap`, `tabLineWidth`,
`tabArrowSize` and `tabArrowGap` are gone; `tabIconSize` and `tabIconGap`
are new. Home is left with the media and timer widgets. Digits 1–9 on Home
(decision 033) no longer reach Search or the theme switch: Double Ctrl and
their buttons do.
