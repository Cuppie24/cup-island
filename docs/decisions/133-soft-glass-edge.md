# 133: The glass's cut is softened

Status: Accepted; the first Windows run refused the blur, see below (amends 089, 097)

**Context.** On a light desktop the island's rounded corners looked
pixelated, much more so with glass on. A screenshot showed why: along the
curve the outermost pixels jump straight from a grey band (~75) to the
desktop (~230), with almost no values between. The tint over the glass (the
twin's layer, decision 097) is antialiased, but the glass under it is cut by
the twin's snapshot, and that cut comes out hard: each edge pixel is either
all glass or none. Where the tint covers a pixel only partly, the glass under
it still fills it, so the hard cut shows through as a staircase. Decision
097's "dark fringe stepping like jaggies" was likely the same hard cut.

**Decision.**
- The cut's mask is the snapshot **blurred by `glassEdgeSoftness`** (0.6 px,
  a Gaussian blur's standard deviation; a length, so scaled with the
  display): the glass fades out across the edge pixel, as the tint over it
  does, instead of stepping.
- The blur is an effect brush of its own, chained into the cut as its
  `Mask` (`softenedMask`, platform/glass.cpp): Windows takes no nested
  effect, but takes chained brushes (decision 100).
- If Windows refuses the blur, the log says "glass: the cut stays hard" and
  the plain snapshot is the mask, as before. 0 turns it off.
- A new softness from tokens.json makes the mask and the glass's brush again,
  like a new darkening (decision 100).

**Consequences.** The glass reaches a fraction of a pixel further out, very
faintly, where the blur spreads. The shadow keeps the plain snapshot: it's
blurred anyway. Needs a Windows run: the corners on a white desktop, glass on,
at 100% scaling; if still stepped, try 0.8–1.

**The first run (0.12.3):** the corners were unchanged. The log said
`glass: the cut stays hard: no blur for it (0x80004002)`: Windows refused
the blur, with Quality optimization, so the mask was the plain snapshot.
Now each guess is tried in turn: Balanced with a Soft border (Win2D's
defaults, which Composition is known to take), then Speed, then the amount
alone. The log names the one taken (`glass: the cut is softened (…)`) or,
for each refused, the step (factory, brush, mask) and the code.
