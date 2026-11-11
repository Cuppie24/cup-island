# 090: Glass with music colours: a Midnight island, the cover glowing from below

Status: Accepted (amends 054 for glass; the glow's lightness amended by 098)

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
  accent, `glassGlowOpacity` (0.8) strong along the bottom, fading to
  nothing `glassGlowHeight` (40 px) up. The fade follows a curve,
  `(1 - up)^glassGlowCurve` (2.5, `glowFade`), drawn as 9 gradient stops:
  steep at the edge so the colour hugs it, flat at the top so it melts into
  the island with no line.
- **A fixed height, not a share of the island's.** A share (the first try,
  0.3) made a tall band on the card and a thin line on the pill and alerts,
  which looked bad; for a while the pill and alerts went back to the cover's
  colours whole and only open cards glowed. With a fixed height the band is
  the same everywhere, so the pill, alerts, the Split bubble and floating
  cards all glow again (on the small pill it fills most of the height).
- Everything else that follows the cover (text, the accent on progress bars
  and the waveform, tiles, hover) still does (`coverColors`).
- Light themes have no glass (decision 089) and keep decision 054's colours.
- The liquid neck between pill and bubble is Midnight, with no glow.

**How.** `glassCoverColors` (core/cover_palette) is `coverColors` with the
island set back to Midnight's and the glow added; `app/live_tokens` uses it
when `withGlass` would draw glass. `islandGlowColor` is a colour token not
in tokens.json, clear unless set, like `islandSecondColor`. The glow is a
vertical gradient on the background's own outline (so it has the island's
shape); its stops' offsets are expressions of that outline's height (a
radius taller than the body while it hangs from the screen edge), so the
band stays `glassGlowHeight` px. If Windows won't let a stop's offset follow
an expression, it's logged and the stops stay at a fixed share.

**Consequences.** Tested in `tests/cover_tests.cpp` (a Midnight island,
one colour, the glow's hue and strength, text readable over it, the accent
still the cover's) and `tests/tokens_tests.cpp` (the fade's curve). How it
looks needs a Windows run; the four numbers are live tokens.
