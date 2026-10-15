# 055: Music colours stay one switch, off by default; no blurred cover

Status: Accepted (changes 052's default)

**Context.** We tried a blurred cover behind the island and splitting the
colours into "Colours from music" + "Two-colour gradient" switches. On a
real screen the blurred cover looked worse than the plain two-colour
gradient (decision 054), and a single-colour mode was one more choice
nobody needed.

**Decision.**
- No blurred cover; the code was reverted.
- "Colours from music" is the only switch: on, the island takes the
  cover's two-colour gradient (054); there's no one-colour mode.
- It's **off by default**: on only once saved "on" (`covercolours`).
  Someone who just installed Cup Island gets their theme.

**Consequences.** People who had it on only by default find it off after
updating. Tested in `tests/settings_new_tests.cpp`.
