# 092: Keys drawn as key caps

Status: Accepted

**Context.** The shortcuts page wrote each combination as caption text
("Ctrl+Alt+Q"), easy to read past and unlike how keys look anywhere else.
Its fixed keys' rows were also shorter than the editable ones, and the list
showed only listVisibleRows (4.5) rows of a long page.

**Decision.**
- **`Keys`**, a content item: a combination's text, drawn by the host as
  key caps. `core/keycaps.h` splits it: "+" joins keys ("Ctrl+Shift+Tab":
  three caps), the joining words ("or", "to", "Double", "then") stay plain
  between caps ("Enter or Space"). A cap is its label in caption size plus
  `keyCapPadding` a side, never narrower than `keyCapHeight` (so "N" is
  square), `keyCapGap` apart, `keyCapRadius` corners. Its fill is
  `tileColor` again over the group's (a shade apart in every theme, no new
  colour) and its edge `trackColor`, like a swatch's ring.
- **The shortcuts page** shows every combination in caps; "Press keys…"
  stays text. Every grouped row is one height (decision 090), so the fixed
  keys' rows match the editable ones. The page is one list as tall as the
  card allows (`listCardVisibleRows`), like Settings' root page.

**Consequences.** Splitting and sizing in `tests/group_tests.cpp`, the
page in `tests/shortcut_settings_tests.cpp`. The look needs a Windows run.
