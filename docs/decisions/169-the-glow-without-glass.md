# 169: The cover glows on a solid island too, and round itself

Status: Accepted (amends 054, 090, 094). Needs a Windows run.

**Context.** With glass off, "Colours from music" still painted the whole
island in the cover's colours (decision 054); only glass had the Midnight
island with the cover glowing from its bottom edge (decisions 090, 166).
Asked for: the same glow with glass off, and a glow round the cover's
picture while it plays.

**Decision.**
- **A dark theme glows, glass or not.** `islandCoverColors` (core) picks:
  a dark island takes `glassCoverColors` (a Midnight island, the glow, an
  open card's lighter glow and its edge line); a light theme keeps
  `coverColors`, the cover's colours whole, since the glow is light on a
  dark island and has no light counterpart. Solid, there's no edge line: it is
  the glass's rim in the glow's colour, a solid island has no rim, and
  tried there the bright line looked stuck on. The glow itself is the same.
- **The cover's shadow turns into its glow.** While a cover's glow is set,
  the card's cover (decision 094's raised art) casts `artGlowColor`: the
  card glow's colour, `coverArtGlow` (0.75) strong, `artGlowBlur` (22) soft,
  straight under it so it shows all round (`artShadow`, core). Cover colours
  are only on while music plays (decision 051), so the glow comes with play
  and gives way to the plain shadow on pause, fading like every recolour.

**Consequences.** A solid dark island no longer takes two colours as a
gradient (decision 054's): that is now light themes' only. Tested in
`tests/cover_tests.cpp` (every theme with glass off; the cover's glow, its
colour and softness; none on a light theme). The two numbers are live
tokens. How it looks needs a Windows run.
