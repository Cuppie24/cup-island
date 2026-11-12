# 101: A button's filled circle is a shape, so it fades with the colours

Status: Accepted; built, not yet run on Windows (amends 094's painted fills)

**Context.** Colours fade from one cover (or theme) to the next (decisions
051, 053): every shape's colour brush animates over `recolorDuration`.
Text, icons and buttons are pixels painted into a surface and are redrawn
in the new colours at once; while they were glyphs and text, nobody
noticed. The media card's hero play button (a large circle filled with
the cover's colour) and the shuffle and repeat toggles' "on" circles
(decision 094) were painted the same way, so with every new cover they
jumped while the island around them faded.

First tried: crossfading every painted element (a fresh surface under,
the old one fading off over it). On screen it looked like the new state
laid over the old, which then slowly went: two copies, not a colour
changing. Taken out.

**Decision.** The filled circles are **shapes**, like the progress bar and
the switches: an ellipse with a colour brush, under the button's painted
glyph, in the glyph surface's whole pixels (`syncFill`). Its colour
(`buttonFillOf`): a primary button's in the button colour, softer under
the pointer (decision 066); a toggle's `tabSelectedColor` while on and
clear while off. It fades over `recolorDuration` on a recolour and over
`hoverFadeDuration` on hover or a toggle switching. It's made the first
time a button has a fill. The glyph is still painted, and redrawn at once:
small, so a jump there doesn't show.

**Consequences.** A primary or toggle button is one shape visual more.
Needs a Windows run: a new cover, the play button and toggles fading with
the island; hovering the play button; switching shuffle on and off.
