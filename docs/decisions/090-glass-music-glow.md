# 090: Glass with music colours: a Midnight card, the cover glowing from below

Status: Accepted (amends 054 for glass)

**Context.** With glass on (decision 089), "Colours from music" still
painted the whole island in the cover's colours, two of them as a left to
right gradient (decision 054). Over the blurred desktop that was a lot of
colour, and a theme's own tint (Grape, Lagoon…) mixed into it.

**Decision.** When the island is glass and a cover's colours are on:
- The **pill, alerts and the Split bubble** are the cover's colours whole,
  as when solid (decision 054), under glass. They're small: the colour is
  what says what's playing.
- An **open card** (Expanded, and floating cards) is **Midnight**: the
  colours in tokens.json as written (the default theme), whatever theme is
  picked, so any cover's colour fades into a neutral card. One colour, no
  gradient. The cover's **main** hue (never the second) **glows up from its
  bottom edge**: `islandGlowColor`, at lightness `coverDarkGlow` (0.45: dark
  enough under white text, tested for every dark theme) and as vivid as the
  cover's accent, `glassGlowOpacity` (0.8) strong along the bottom,
  `glassGlowMiddle` (0.25) of that halfway up, nothing `glassGlowHeight`
  (32 px) up. A fixed height, not a share: the same small band on a short
  card and a tall one; the faint middle stop makes the fade soft, the colour
  hugging the edge (the user asked for a shorter, softer band after a first
  try at 0.3 of the height).
- Switching between the two (pill to card and back) fades over
  `morphDuration`, with the morph, not over the slower recolour.
- Everything else that follows the cover (text, the accent on progress bars
  and the waveform, tiles, hover) still does (`coverColors`).
- Light themes have no glass (decision 089) and keep decision 054's colours.
- The liquid neck between pill and bubble is the pill's colour.

**How.** `glassCoverColors` (core/cover_palette) is `coverColors` plus the
card's own colour (`islandCardColor`, Midnight's) and the glow; `app/
live_tokens` uses it when `withGlass` would draw glass. Both are colour
tokens not in tokens.json, clear unless set, like `islandSecondColor`;
clear `islandCardColor` means a card is coloured as the pill. `Blob::
setCard` (from `PillRenderer::apply`: Expanded, or always when floating)
picks which. The glow is a vertical gradient on the background's own
outline (so it has the island's shape); its stops' offsets are expressions
of that outline's height (a radius taller than the body while it hangs from
the screen edge), so the band stays `glassGlowHeight` px. If Windows won't
let a stop's offset follow an expression, it's logged and the stops stay at
a fixed share.

**Consequences.** Tested in `tests/cover_tests.cpp` (the pill as when
solid, a Midnight card, the glow's hue and strength, text readable over it, the accent
still the cover's). How it looks needs a Windows run; the four numbers are
live tokens.
