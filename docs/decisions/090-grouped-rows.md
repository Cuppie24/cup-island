# 090: Grouped rows: quiet labels, one background per group, two-level items

Status: Accepted (changes 037's headings; supersedes 030's value at a row's end on Settings' list)

**Context.** Settings' pages were rows on the bare island under bold,
full-strength headings (decision 037). With nothing around a group, the
heading had to do all the work of setting it apart, and a setting's value
sat at the row's far end, a long way from the name it belongs to. The look
wanted is the one of Windows' and phones' own settings: a small grey label,
the group's rows together on one rounded background, each row a name with
a quieter line under it and its control at the end. And it shouldn't be a
Settings-only look: any card with groups of rows should get it for free.

**Decision.**
- **`Row::grouped`** (content model, host-drawn). Rows back to back with it
  sit on one background: `tileColor` (Home's tiles' surface, so every theme
  already has it), `groupRadius` corners. A row set apart (`section`,
  `apart`) or in another list starts a new group. Each grouped row has
  `groupItemPadding` above and below (roomier than a list item's), is at
  least `smallButtonSize` tall whatever it ends in (a switch, or no control,
  is shorter than a button: Start with Windows looked squashed), and its
  content inset like a list item's.
- **Drawn as one piece per row** (`GroupFill`, a host-only item), from under
  the row's gap to the next row's top, so pieces meet with no seam, rounded
  only at the group's ends. Per row, not one shape per group, so a piece
  scrolls, clips and is dragged with its row in a list like the rest of the
  row (decision 029). It is placed last and inserted at the bottom of its
  parent, so it stays behind its row's content even when rebuilt, and has no
  click area: the row's control still takes the clicks. A grouped list item
  lights within its own piece, under the translucent fill.
  A piece's height comes from its row's own height (plus the row spacing
  when the group goes on), never from two scrolled tops: their difference
  moved by a float's rounding as the list scrolled, and since only a real
  size change rebuilds an element (decision 071), every piece was rebuilt
  and faded in on each scroll step: they flickered.
- **Two levels**: the description is the row's `Text::detail` (decision
  079's second line), with `detailTrimsEnd` so a long one ends in "…" like
  any text, not at its start like a path. A text's natural width is its
  wider line, so a short name never cuts its description.
- **Labels are quiet**: `TextStyle::Heading` is now the caption colour,
  `headingFontSize` 14 (12 read as too small on screen), weight 600, sitting at the group's edge. The
  background separates the groups; a loud heading on top of it was double.
- **Builders** in `core/groups.h`: `itemText(name, description)`,
  `groupItem(text, end)` (a Button or Switch at the end makes the whole row
  its button and lights it), `groupLabel`, `addGroup(slot, label, rows, list)`.
  Settings uses them on every page: a setting's value is now its
  description ("Theme / Midnight", "Check for updates / Version 0.6.14");
  the shortcuts page keeps its keys at the row's end, a table's column.

**Consequences.** Rows are taller: the usual Settings page still shows
whole, and with everything on it (two monitors, an update offered) it
scrolls in a card that fits the canvas (`tests/settings_tests.cpp`). The
builders and the layout (pieces, seams, insets, no clicks) in
`tests/group_tests.cpp`. The look needs a Windows run.
