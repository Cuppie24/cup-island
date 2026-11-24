# 171: A recolour eases off the mark

Status: Accepted; built, not yet run on Windows (amends 051, 053)

**Context.** When music stops, the island's colours fade from the cover's
back to the theme's (decision 051) over the picked speed (decision 053:
0.8, 2 or 4 seconds). The start of that fade felt sharp, even at Slow: the
colours seemed to jump, then drift. `fadeColor` gave its key frame no
easing, and with none Composition uses a curve of its own, which does most
of its changing early. Over a hover's 120 ms that reads as a prompt answer;
over seconds it reads as a jump followed by a long tail.

**Decision.**
- A recolour runs along **`recolorEase`** (a token, `[0.55, 0, 0.45, 1]`):
  at rest at both ends, slow off the mark, quickest in the middle, slow to
  land. The whole fade's time is used, and nothing jumps.
- Only fades **as long as a recolour**: `recolorCurve` (core) gives the
  curve for a duration no shorter than the shortest of the three speeds,
  and none for the quick ones (hover, a switch, a toggle's fill), which
  answer a click and keep the compositor's own curve. One rule in
  `fadeColor`, so every colour that fades with a recolour (the fill, the
  glow, text and glyph ink, a button's fill, the shiny edges) moves alike.

**Consequences.** A recolour's first moments are quieter, so at Fast it may
feel a touch later than before; the curve is live in tokens.json. Opacity
fades (`fadeTo`) are unchanged: they run along `morphEase`, and none of
them is what a cover's colours leaving shows. Tested in
`tests/tokens_tests.cpp`.
