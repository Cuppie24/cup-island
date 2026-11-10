# 090: Glass with music colours: a Midnight island, the cover glowing from below

Status: Accepted (amends 054 for glass)

**Context.** With glass on (decision 089), "Colours from music" still
painted the whole island in the cover's colours, two of them as a left to
right gradient (decision 054). Over the blurred desktop that was a lot of
colour, and a theme's own tint (Grape, Lagoon…) mixed into it.

**Decision.** When the island is glass and a cover's colours are on:
- The island is **Midnight**: the colours in tokens.json as written (the
  default theme), whatever theme is picked, so any cover's colour fades into
  a neutral island. One colour, no gradient.
- The cover's **main** hue (never the second) **glows up from the bottom
  edge**: `islandGlowColor`, at lightness `coverDarkGlow` (0.45: dark enough
  under white text, tested for every dark theme) and as vivid as the cover's
  accent, `glassGlowOpacity` (0.85) strong along the bottom, fading to
  nothing `glassGlowHeight` (0.3) of the way up. It stays well below the
  middle: the user asked for a small band.
- Everything else that follows the cover (text, the accent on progress bars
  and the waveform, tiles, hover) still does (`coverColors`).
- Light themes have no glass (decision 089) and keep decision 054's colours.
- The pill, the Split bubble and floating cards all glow; the liquid neck
  between pill and bubble doesn't.

**How.** `glassCoverColors` (core/cover_palette) is `coverColors` with the
island set back to Midnight's and the glow added; `app/live_tokens` uses it
when `withGlass` would draw glass. `islandGlowColor` is a colour token not
in tokens.json, clear unless set, like `islandSecondColor`. The glow is a
vertical gradient on the background's own outline (so it has the island's
shape). That outline reaches a radius above the body while it hangs from
the screen edge, so its clear stop's offset is an expression of the visible
height, and the glow reaches the same share of what shows on the small pill
and the tall card. If Windows won't let a stop's offset follow an
expression, it's logged and the share is of the outline's height instead.

**Consequences.** Tested in `tests/cover_tests.cpp` (Midnight island, one
colour, the glow's hue and strength, text readable over it, the accent
still the cover's). How it looks needs a Windows run; the three numbers are
live tokens.
