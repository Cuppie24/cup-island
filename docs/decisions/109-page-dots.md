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
- **‹ and › right beside the dots**, `tabArrowSize` (24) square,
  `tabArrowGap` (6) from them, the three centred on the card as one group
  (the user's choice: beside the dots, not at the card's edges). They step
  to the page before or after, round the row (`previousTab`, `nextTab`, as
  Ctrl+Tab does). Shown when there's somewhere to go.
- **Home's button stays at the left edge and the pin at the right**, as
  before (decisions 009, 046); Home is also the first dot.
- **The row is `tabHeight` (24) tall**, from 36. A dot's click area is the
  row's height and half the gaps beside it; under the pointer a soft circle
  lights round it (`Tab::lit`).
- A badge on a page sits just off its dot's top-right; Home's button keeps
  its own. The keyboard stops on the dots (not the shown one), the arrows and the pin.

**Not taken.** Arrows at the card's edges: they'd sit where Home's button
and the pin are, and far from the dots they move between.

**Consequences.** Nothing on the row says which activity a dot is: the card
does once it's shown. `tabGap` and `tabMaxWidth` are gone.
