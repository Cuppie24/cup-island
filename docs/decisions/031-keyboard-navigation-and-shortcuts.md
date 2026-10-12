# 031: Keyboard navigation and shortcuts

Status: Accepted; shortcuts superseded by 032 (system-wide, rebindable, Ctrl+Alt+Space)

**Context.** Everything on the island needed the pointer. Settings (decision
030) became a list of rows and a page of themes, which people expect to walk
with the arrow keys, and Home is a grid of widgets. There was also no way
to get to Home without clicking the pill, and no place that says which keys
do what.

**Decision.**
- **Arrow keys move a focus** over what can be clicked on the open card or
  Home: buttons, fields and clickable art (the same items `hitTest`
  answers for), Home's widgets that open or toggle, and the tabs except the
  one shown. Where an arrow goes is pure (`core/keyboard`, `moveFocus`):
  the nearest one that way. Left and right stay on the row; up and down
  favour what's in line, then the nearest off to the side (a tab under
  the buttons). Nothing that way: focus stays. The first press lands on
  the first one, top left, so it shows where focus is before moving it.
- **Focus looks like a hover.** The app keeps the focused one's id and
  feeds its point to the hover code instead of the pointer's, so a focused
  row, button, tile or tab lights exactly as under the pointer: nothing new
  to draw or theme. A real pointer move takes the lead back (Windows
  repeats a move when windows change under a still pointer; that one
  doesn't count).
- **Enter or Space clicks it**, through the same functions as a pointer
  release (`clickTab`, `clickTile`, `clickElement`), so a key can't do
  anything a click can't. If that opened something else (a card, a page),
  focus moves to its first item. A list row out of its window is scrolled
  in when focus lands on it (`ListRevealed`, decision 029). Keys count as
  using the island: Expanded doesn't time out under them.
- **Shortcuts are one table** (`core/shortcuts`): each line has the keys as
  written and what they do, and a chord and a command if it's a shortcut
  rather than a description of the arrows, Enter and Esc. The app runs
  `commandFor(chord)`; Settings lists the same table on a "Keyboard
  shortcuts" page. First: **Ctrl+X opens Home.**
- **Not system-wide.** Like the demo keys and Esc, they work while the
  island has keyboard focus (it gets it when clicked). A global Ctrl+X
  (`RegisterHotKey`) would take Cut from every other app. A system-wide
  shortcut, if wanted, needs a chord nothing else uses.
- The demo keys (M, T, C, D, H) now only fire without Ctrl or Alt.

**Consequences.** Navigation and the table are tested in
`tests/keyboard_tests.cpp` (Settings' rows and buttons, the theme page's
list past its window, Home's grid), the Settings page in
`tests/system_tests.cpp`, Home from anywhere in `home.scenario`. The key
handling and the look of focus need a Windows run.

**Open.** Tab and Shift+Tab don't move focus; there's no focus ring
distinct from hover.
