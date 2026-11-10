# 091: One updates row: Check for updates turns into Update and restart

Status: Accepted (changes 037's separate Update and restart row)

**Context.** With an update offered, Settings showed two rows: Check for
updates ("0.5.1 available") and, under it, Update and restart (decision
037). The second one only repeated the first's news, and the check row
stayed clickable for a check nobody needed any more.

**Decision.** One row, two states:
- **Nothing offered**: "Check for updates", a refresh glyph (`Glyph::Refresh`),
  and under it what the last check found, until then "Version <running>".
  A click checks.
- **An update offered** (by this row's check or the daily one): the same row
  says "Update and restart", "<running> → <offered>" under it, and the
  download glyph. A click installs it, like Home's Update card. Under way it
  says "Installing…"; failed, "Try the update again" and why. Withdrawn, it
  goes back to checking.

**Consequences.** Settings' fullest page is a row shorter. Tested in
`tests/settings_tests.cpp`. The refresh glyph (U+E72C) is in Segoe Fluent
Icons and Segoe MDL2 Assets alike, so Windows 10 has it.
