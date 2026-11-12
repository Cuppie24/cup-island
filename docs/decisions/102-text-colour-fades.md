# 102: One-colour text and icons are painted white and coloured by a fading brush

Status: Accepted; built, not yet run on Windows (extends 101)

**Context.** With the buttons' filled circles made shapes (decision 101),
the controls faded to a new cover's colours, but the text didn't: text is
pixels in a surface, redrawn in the new colour at once. Crossfading two
painted copies was tried and looked like one laid over the other (101).

**Decision.** A text drawn in one colour is painted **white**, and its
sprite shows it through a **mask brush**: the source a colour brush in the
text's colour, the mask the painted surface. A recolour fades that colour
brush (`fadeInk`) over `recolorDuration`, like any shape; the white pixels
stay. A change of style (another colour) fades over `hoverFadeDuration`.

Not one colour, so still redrawn at once: bright digits or bright parts
(decisions 068, 079), a detail line in caption colour (079), and an emoji,
which has its own colours (064; any 4-byte character counts). A text that
gains or loses a second colour is rebuilt.

**Then icons and swimming text too.** The same for an icon (not the 🔥
emoji) and a button's glyph or label (`inkOf`: as paint() draws it; not a
round button, two colours, or a result's picture, or a label with an
emoji): white pixels, a brush's colour. A secondary button's grey-to-full
on hover now fades too. Swimming text already shows its pixels through a
mask brush (its soft edges), and Windows takes no mask brush inside
another, nor (decision 100) an effect inside another: so it's one
Composite effect, source-in over three layers (the edges' gradient, the
scrolling text, the colour). If Windows refuses that, it's logged once and
swimming text keeps its own colour, redrawn at once. The effect
description (`Effect`) moved from glass.cpp to platform/effects so both
use it.

**Consequences.** One mask brush per one-colour text, no extra surface.
Needs a Windows run: a new cover's text colours fading with the island.
