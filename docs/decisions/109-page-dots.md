# 109: The tab row is page dots with arrows

Status: Accepted (amends 007, 047, 072: tabs are no longer drawn). Core tested; the dots, the gliding line and the arrows on Windows need a run.

**Context.** Expanded's tab row showed a tab per activity, its icon or art
and a short title, 36 px tall (decisions 007, 047). With the cards now one
height and the card itself saying what it is, the user asked for a quieter
row: no tabs, small dots with a line for the shown page, an arrow on each
side, and a shorter row.

**Decision.**
- **A dot per page**, Home first (with panels), then activities in start
  order: `tabDotSize` (6) round, `tabDotGap` (6) apart, faint
  (`stepTodoColor`). The shown page is a line, `tabLineWidth` (18) long, in
  the theme's text colour. Picking another page, the line glides there and
  grows or shrinks to its new length (the settle curve, as the tab marker did).
- **‹ and › right beside the dots**, `tabArrowSize` (32) round,
  `tabArrowGap` (4) from them, the three centred on the card as one group
  (the user's choice: beside the dots, not at the card's edges). They step
  to the page before or after, round the row (`previousTab`, `nextTab`, as
  Ctrl+Tab does). Shown when there's somewhere to go.
- **Home's button stays at the left edge and the pin at the right**, as
  before (decisions 009, 046); Home is also the first dot.
- **The row is `tabHeight` (32) tall**: its buttons' height, so their hover
  shapes are a good size. It sits `tabRowInset` (12) from the card's edge,
  just inside the top band (decision 108), and `tabRowGap` (6) over the
  content: little room above or below it, so it reads as part of the
  card's edge, not a bar with space under it (24 tall, at the card's
  padding with slotGap under it, was tried first: the hover shapes were
  small and there was space under them). A dot's click area is the row's
  height and its width plus `tabDotGap` (10); under the pointer a soft
  circle lights round it (`Tab::lit`).
- **Grey until pointed at**: the arrows' glyphs and the shown page's line
  are in `buttonOffColor`, like secondary buttons; an arrow brightens only
  under the pointer. Each glyph in the row is keyed to its own button
  (`kHomeTab`, `kPinTab`, `kPreviousTab`, `kNextTab`), so hovering a dot
  lights nothing else (Home's button flickered with Home's dot, whose id
  is the empty one, on the first run).
- A badge on a page sits just off its dot's top-right; Home's button keeps
  its own. The keyboard stops on the dots (not the shown one), the arrows and the pin.

**Not taken.** Arrows at the card's edges: they'd sit where Home's button
and the pin are, and far from the dots they move between.

**Consequences.** Nothing on the row says which activity a dot is: the card
does once it's shown. `tabGap` and `tabMaxWidth` are gone.
