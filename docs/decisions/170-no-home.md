# 170: No Home; every card is reached from the page row

Status: Accepted (supersedes 009's Home, 019, 073, 110, 114 and 115; amends
033: Esc, the digits; 032: the Ctrl+Q shortcut; 111: a widget's sizes).
Core tested; the look on Windows needs a run.

**Context.** Home was a card of widgets, one per provider, and the way to
every dormant provider's card. With the page row a constant list of
providers' icons (decision 165), with Search, the theme switch and Settings
as buttons in it, every card and toggle is already one click from any
card. Home showed the same things a second time. The user asked for it to
go, with nothing depending on it.

**Decision.**
- **No Home**: no widget grid, no Home button, no `homeOpen`. Expanded
  always shows a card. Search's button takes the row's left corner
  (`Panel::RowSpot::LeftCorner`).
- **A click on the idle pill** opens the default page's card: the first
  panel with a card, in publish order (media's "Nothing playing"). With
  something running it opens the pill's activity, as before. With no page
  at all the click does nothing.
- **Ctrl+Q is "Open the island"** (`Command::OpenIsland`, `IslandOpened`):
  the default page, from anywhere; pressed again while a card is open, it
  closes. Its saved name stays `home`, so a rebound key survives. Launching
  the app again does the same.
- **Esc closes.** A card no longer remembers being opened from Home; a
  card's own step back stays its provider's (`Content::backAction`).
- **`CardOpened`** (was `TileClicked`) opens a card by id from anywhere: a
  page-row button with no tap action, Search's shortcut.
- **The page row shows** with two pages or more, or any panel's button, so
  Settings stays at hand on a lone card.
- **Gone with it**: `Content::tile` and `tileLeading`, `Panel::wide`,
  `Panel::announce` with the "New" tag on a widget and its saved list
  (decision 110), `TextStyle::TileTitle`, `Icon::quiet` and `Icon::large`,
  the 1–9 keys, and the tokens `homeColumns`, `tileHeight`, `tileGap`,
  `tilePadding`, `tileIconSize`, `tileTitleFontSize`,
  `tileTitleFontWeight`. `homeButtonWidth` is `rowButtonWidth`.
  `tileColor` stays: grouped rows and windows use it.

**Not taken.** Remembering the last card for the idle click: the island
would open on something different each time. Keeping the widgets' live
glance (a cover, a ring) in the row's icons: decision 165 keeps the list
the same whatever plays.

**Consequences.** A dormant provider's glance (the planned focus time,
"Not playing") is seen only on its card. An update still shows as the dot
on Settings' gear (decision 152). The saved `seenPanels` list is no longer
read; it is left where it is.
