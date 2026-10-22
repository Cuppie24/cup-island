# 064: Primary and secondary buttons, tables, dividers, more room

Status: Accepted (amends 063)

**Context.** A mockup of a decluttered timer showed what the card was
missing: one obvious main action, grey controls that light up when pointed
at, statistics in columns (a value over what it is), a line between groups,
and more room around everything. Each is something any card could use, so
each went into the host, not into Pomodoro.

**Decision.**
- **`Button::primary`:** the card's one main action, a filled circle
  (`primaryButtonSize`, glyph `primaryGlyphSize`) in the button colour, its
  glyph in the island's. Pomodoro's ▶ and ⏸ (pause in the middle of stop
  and skip). Media keeps its controls as they are.
- **`Button::secondary`:** its glyph in `buttonOffColor` until pointed at,
  then `buttonColor`. The gear, the pages' back arrows, the plan's − and +,
  stop and skip. Main controls stay bright.
- **Tables are rows with `Row::even`:** items share the row's width equally,
  each centred in its share, so rows with the same count line up as
  columns: a value row over a caption row. A table item with cells holding
  rows was considered: it nests the content model (rows inside items),
  which keyboard focus, hit-testing and element keys all assume is flat.
  Even rows give the same grid and keep it flat.
- **`Divider`:** a thin line (`dividerThickness`, the track colour) across
  its row; sets a group apart more than `Row::section`'s gap.
- **Colour emoji in text:** text is drawn with colour fonts on, so "🔥 3"
  and "🏆" show in their colours.
- **More room, everywhere:** `expandedPadding` 12 → 18. Content corners
  nest in the card's (decision 006: radius minus padding), so
  `maxCornerRadius` goes 28 → 34 to keep them as round, and the media
  card's fixed height `expandedSize.y` 159 → 171 to keep its bottom margin.
- **Pomodoro:** the plan moves to the top of its settings page (the
  hover-revealed − and + of 063 are gone with it; dimmed while a session
  runs). The dormant card: days in a row, the big time, "Focus" under it,
  ▶, today's line, the to-dos. Statistics: "Today" (cycles, focus time, days
  in a row), a line, "🏆 Personal records", each a three-column table;
  a value beating its record has a "Record" tag in place of its caption.

**Not taken from the mockup.** A ring around the time (014 dropped a ring
by the digits; a ring with content inside would be a new item, see below),
a fixed-height card with rows pinned to its bottom (cards grow with their
to-dos, 016), a single to-do instead of the list (016, 029), its own font
and line icons (024: Windows' own), and fading Home and the pin (the tab
row is every card's, 046, 047).

**Open.** A ring with a countdown inside, as one item (no nesting), if the
card wants it back.
