# 054: Cover colours as vivid as the cover, and a two-colour gradient

Status: Accepted (amends 051)

**Context.** The island in the cover's colours (decision 051) looked
lackluster: only the cover's hue was used, at one fixed, cautious strength
(island chroma 0.05 on a near-black), so a neon cover and a dusty one came
out equally dull, and every song looked much the same.

**Decision.**
- **As vivid as the cover.** `coverTint` measures how vivid the cover's main
  colour is: its OKLCH chroma over `coverFullChroma` (0.2), 0..1. The
  island's and the accent's chroma run between a muted and a vivid value,
  `coverIslandChroma` [0.04, 0.12] and `coverAccentChroma` [0.12, 0.22], by
  that amount. The island is a little lighter too (`coverDarkIsland` 0.24,
  `coverLightIsland` 0.92), so it can hold more colour.
- **A second colour, as a static gradient.** When the cover has a second
  hue at least 45° from the main one, with `coverSecondShare` (30%) of its
  weight, the island runs from the main colour on the left to the second on
  the right (`islandSecondColor`, at the same lightness and strength, so
  text reads the same on both ends; tested for every shipped theme). A stray
  accent or a neighbouring hue (red and orange) stays one colour. The
  gradient is a `CompositionLinearGradientBrush` that doesn't move: only a
  recolour animates its two stops, like the solid fill before. Decision 051
  ruled out gradients because a *moving* one re-renders every frame; a
  still one costs nothing between changes. Each flared corner takes its
  end's colour, so they join without a seam.
- `islandSecondColor` is a colour token but not in tokens.json: the app
  sets it to `islandColor` under every theme, and a two-colour cover
  replaces it (`app/live_tokens`).
- `Content::tint` is a `CoverTint` now: main colour, optional second, vividness.

**Consequences.** Tested in `tests/cover_tests.cpp` (two colours or one,
vividness, readable text on both ends in every theme). The liquid neck
between pill and bubble stays the main colour. How it looks needs a Windows
run; the strengths are tokens, live-tunable.
