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
  colours as written. Dark themes first, then light ones.
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
tab's mark), and a pale tint of it for text and icons. The shadow stays
Midnight's black in every theme: a coloured shadow, even a faint one, reads
as a halo around the island rather than a shadow under it.
Grape (purple), Lagoon (teal), Ember (orange), Lime (acid green), Bubblegum
(hot pink), Sapphire (electric blue). An activity's own accent (Pomodoro's
red and green, decision 010) still wins over the theme's.

**Consequences.** Parsing in `core/tokens_json.cpp` (the reader moved to
`core/json_reader.cpp`), tested in `tests/tokens_tests.cpp`, which also
checks every shipped theme is dark. The card in `tests/system_tests.cpp`.
The look of each theme needs a Windows run.

**Light themes** (Paper: white with a blue accent; Sorbet: pastel peach with
a watermelon accent) flip the recipe: a pale island, dark ink for text and
icons, the accent for fills and faint tints of it behind tiles and tabs, a
deeper red for `dangerColor`, and dark ink for the progress knob (white
would vanish). The shadow stays black but fainter, 20% instead of 45%: it
falls on whatever is behind the island, not on the island, and a pale island
over a white page at 45% reads as a dark smudge. Light interfaces use faint
shadows for the same reason. Black is the rule; only the strength changes. The one thing that had to change:
clickable art's "open" glyph sits on art dimmed with black, so it got its own
`artHoverGlyphColor` (light in every theme) instead of `buttonColor`. The
test checks every shipped theme's text against its island (WCAG contrast:
title 7:1, body 4.5:1, caption 3:1), dark or light.
