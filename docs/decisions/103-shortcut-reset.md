# 103: Shortcuts reset to their defaults, one or all

Status: Accepted (extends 032)

**Context.** Decision 032 lets every command's keys be changed, but the only
way back to the default was to remember it and record it again. Swapped keys
(Home on Ctrl+Alt+Q, the activity on Ctrl+Q) couldn't be undone that way at
all: each default is the other command's keys, so either change is refused.

**Decision.**
- **Each command's row has a reset button** (Undo glyph, before Edit), on
  every row so the buttons line up, dimmed while its keys are the default.
  It goes through the same checks as recording (`SettingsProvider::rebind`):
  if the default is another command's keys, or another app has them, the old
  keys stay and the note says why.
- **"Reset all to defaults"**, the last row under "Anywhere", dimmed when
  nothing differs. It doesn't go row by row: every hotkey is let go and the
  defaults (which never clash among themselves) registered afresh, so swapped
  keys come back too. One another app has is kept as the default anyway, as
  at startup, and the page says it's taken. No confirmation: it's one click
  to change back, and a mistaken reset loses at most a few chords.
- Either one stops a recording under way.

**Consequences.** `Glyph::Reset` (MDL2 E7A7, Undo; in Windows 10's font too).
Tested in `tests/shortcut_settings_tests.cpp`.
