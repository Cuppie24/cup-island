# 022: A Settings widget, and app themes

Status: Accepted

**Context.** Home had a "Copy logs" tile: a tool for bug reports taking a
widget's place next to things used every day. The island also had one look,
black and white, and people want to make it theirs. Both need a home that
isn't another tile per option.

**Decision.**
- **Settings is a widget** (`providers/system/settings_provider.h`, pure),
  last on Home: a gear, "Settings". A click opens its Centred card. Like
  every widget it only informs (decision 019); the controls are on the card.
- **Copy logs moved into the card**: a text button that says "Logs copied"
  (or "Couldn't copy logs") for `logsNoteMs`, then "Copy logs" again. The
  tile is gone.
- **App themes are colour sets in tokens.json**, under `"themes"`: a name and
  the colour tokens it changes from the file's own values. Only colour tokens
  (a theme recolours, never resizes), checked like any token, with line
  numbers. The first is the default; "Midnight" changes nothing, so it's the
  colours as written. Dark themes only for now.
- **The pick is applied over the file** (`withTheme`, then display scaling),
  by `app/live_tokens`. `tokens()` is always what's on screen, so the
  renderer needs nothing new: a theme change is a token change, recoloured
  live like a tokens.json save.
- **The card picks with arrows** around the theme's name, wrapping at the
  ends. The island recolours at once: the island is the preview.
- **Saved per user**, in `%LOCALAPPDATA%\CupIsland\theme.txt` (the name),
  beside the to-dos. A saved name tokens.json no longer has falls back to the
  first theme; so does a tokens.json save that renames or drops it.
- **This is not the Windows theme widget.** That one switches Windows' own
  light/dark mode; app themes only colour the island.

**The shipped themes.** Each is built from four colours: a near-black base
for the island, an accent (progress fills, waveform, tile and hover tints, a
tab's mark), a pale tint of it for text and icons, and a glow for the shadow.
The glow is softer than Midnight's black shadow (alpha 0.28, not 0.45): a
coloured shadow reads much stronger than a black one of the same alpha, and
at full strength it looked like a halo, not a shadow.
Grape (purple), Lagoon (teal), Ember (orange), Lime (acid green), Bubblegum
(hot pink), Sapphire (electric blue). An activity's own accent (Pomodoro's
red and green, decision 010) still wins over the theme's.

**Consequences.** Parsing in `core/tokens_json.cpp` (the reader moved to
`core/json_reader.cpp`), tested in `tests/tokens_tests.cpp`, which also
checks every shipped theme is dark. The card in `tests/system_tests.cpp`.
The look of each theme needs a Windows run. Light themes later would need
dark text tokens checked against every surface (art hover, dialogs).
