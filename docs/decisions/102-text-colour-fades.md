# 102: One-colour text is painted white and coloured by a fading brush

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
which has its own colours (064; any 4-byte character counts). Swimming
text keeps its marquee brush and is redrawn at once too. A text that
gains or loses a second colour is rebuilt.

**Consequences.** One mask brush per one-colour text, no extra surface.
Needs a Windows run: a new cover's text colours fading with the island.
