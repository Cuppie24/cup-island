# 150: Windows' own theme

Status: Accepted (amends 104). Core and Settings tested; the colours need a Windows run.

**Context.** Providers' windows (Settings, Pomodoro's) were always drawn in
the island's colours: its theme, and music's colours over it. The user asked
for a setting of their own: like the island, like Windows' light or dark
mode, or a theme picked for the windows.

**Decision.**
- **Settings**: a Window theme row on Appearance, under Theme, saying what
  it's set to ("Like the island", "Like Windows", or the theme's name). It
  opens a page within Appearance: Colours, Island | System | Manual picked in
  place (Segments, decision 093). The themes, Dark then Light as on Theme's
  page, are listed under it only while it's Manual. Manual starts at the
  island's theme, so switching to it changes nothing until one is clicked.
  Stored as `windowtheme` (island, system, manual; nothing is island) and
  `windowthemename`. Announced as new (decision 048).
- **Which theme** (`providers/system/window_theme.h`, pure): Island, none of
  their own; System, the first light or first dark theme in tokens.json's
  order as Windows' apps are (`AppsUseLightTheme`, there since Windows 10
  1607), none of that kind falling back to the island's; Manual, the picked
  one, or the first if tokens.json lost it.
- **Drawing**: `live_tokens` keeps a second set beside tokens(): the file's,
  the window theme's colours over them, the same scale and picks, no cover
  colours (those are the island's). Every painter reads tokens(), so while a
  window paints (draw, hover, press, focus ring, refresh, glass) its set
  stands in for the island's and is put back right after (`WindowColours` in
  `app/provider_windows.cpp`). Only around painting: layout reads lengths,
  which are the same in both, and what a click routes may redraw the island
  at once, which must stay in its own colours.
- **Windows' mode changing**: the island already hears "ImmersiveColorSet"
  (Glass, decision 089); it re-resolves the window theme too, so System
  follows a switch at once.

**Consequences.** `tests/window_theme_tests.cpp`. As the island (the default)
nothing changes: no second set, no swap. A window's acrylic is light or dark
by its own island colour, so a light window theme over a dark island gets
the light acrylic.
