# 053: Music colours page, on/off switches, a fade speed to pick

Status: Accepted

**Context.** Colour fades (decision 051) felt too quick at 600 ms, and 1.2 s
was still on the quick side, but how slow is right is a matter of taste.
"Colours from music" sat alone on Settings' list, with its fade nowhere to
be found. On/off settings used a tick box, which reads like a choice in a
list rather than something that's on or off.

**Decision.**
- **A fade speed:** Fast, Medium (the default) or Slow, picked in Settings
  and kept in the store (`fade`: "fast"/"medium"/"slow"; nothing = medium).
  Each is a token in tokens.json: `recolorFastMs` (800), `recolorMediumMs`
  (2000), `recolorSlowMs` (4000). The pick sits on `Tokens::fadeSpeed`, put
  there by `app::setFadeSpeed` like the theme's colours, so it survives a
  tokens.json reload; `recolorDuration(tokens)` gives the duration. It isn't
  something to rebuild for (`onlyColorsDiffer` ignores it).
- **One "Music colours" page:** the list has a "Music colours" row saying
  On or Off, opening a page with the "Colours from music" switch and the
  "Colour fade" choices. The speed also times theme changes, which use the
  same fade; it lives here because music is what changes colours most.
  Announced (decision 048): "musiccolours" and "fade" are new; "covercolours"
  keeps its id, now on that page.
- **Switches instead of tick boxes** for on/off settings (Colours from music,
  Start with Windows): a `Switch` content item, a pill track with a knob
  that slides to the end that's on (`platform/switch_parts`). On, the track
  is the live colour and the knob the island's, so it reads in every theme
  and cover. A click anywhere on the row flips it, as before. Flipping
  slides the knob and fades its colours over `morphDuration`; a recolour
  fades them like any shape. Choices in a list (themes, displays, the tab
  row, the fade) keep their tick: picking one of several isn't on/off.

**Why a fade can start late** (asked while this was built): the island's
colour comes from the new cover, so the fade can only start once the app
has sent its picture and it's been read. Apps send the title first and the
picture later (Chrome, often Spotify), and a new track is held back at most
`mediaArtWaitMs` (1 s) for it. If the picture comes later still, the old
colour stays for `coverTintLingerMs` (1.5 s) and the fade starts when it
arrives. Pausing waits that 1.5 s on purpose before going back to the
theme, so the gap between two tracks doesn't flash it. The log now has
"media: new track" and "media: cover shown" lines, so the wait can be read
off their timestamps.

**Consequences.** Tested in `tests/settings_new_tests.cpp` (the page, the
switch, the speed and their restart), `tests/settings_tests.cpp` (Start
with Windows' switch) and `tests/tokens_tests.cpp` (speeds). The switch's
look and slide need a Windows run.
