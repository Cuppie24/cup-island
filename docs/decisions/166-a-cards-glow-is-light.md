# 166: An open card's glow is light, with a line along its edge

Status: Accepted (amends 090, 098). Needs a Windows run.

**Context.** The glass glow (decisions 090, 098) is drawn at an OKLCH
lightness of 0.45 to 0.56 so white title text keeps 4.5:1 over it. That cap
is for the pill, where the 40 px glow covers most of the height and the
title sits on it. On an open card the same glow read as a dark stain along
the bottom: a mock of the card with the cover's colour as it is (lightness
0.6 to 0.7), a softer fade and a bright line on the edge looked like light
coming off the edge, and the difference was the lightness, the line and
the reach. `(1 - up)^2.5` over 40 px leaves 14% at 20 px: a band about 15 px
tall on a 248 px card.

**Decision.** The glow has two looks (`glowLook`, core/tokens):
- **The pill, alerts, the bubble:** as before.
- **An open card** (`Mode::Expanded`): `islandCardGlowColor`, the same hue
  and chroma `coverCardGlowLift` (0.15) lighter; reaching
  `glassCardGlowHeight` (48 px) along `glassCardGlowCurve` (1.6); and a line
  along the bottom edge, `islandGlowEdgeColor` (`coverGlowEdgeLift`, 0.1,
  lighter again, `glassGlowEdgeOpacity` strong). The line is the glass's rim
  in that colour: its bottom stop, so it fades out up the sides as the rim
  does. `glassGlowEdgeOpacity` 0 leaves the rim white.

`PillRenderer::apply` tells the pill's blob whether it's a card
(`Blob::setCard`); the colours fade and the reach glides over
`morphDuration`, with the card's opening. (A recolour's time was used
first: 2 s at the default speed, so the glow lagged the card.) The bubble
never is one.

**Why it's safe on a card.** Nothing read sits on a card's bottom edge: the
lowest are a control's glyph and a Home widget's second line, a quarter of
the reach up or more. A test holds white to 4.5:1 over the card's glow
there, for nine hues, on glass at its brightest.

**Consequences.** Tested in `tests/cover_tests.cpp` (the pill's glow
unchanged; the card's lighter, same hue and strength, higher, slower; its
line lighter again; no cover, none). All five numbers are live tokens. A
toast floats over a card's bottom on its own background, so it isn't on the
glow. How it looks, and the fade between the two looks as a card opens,
needs a Windows run.
