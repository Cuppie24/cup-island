# 101: Painted elements crossfade to new colours

Status: Accepted; built, not yet run on Windows (amends 051's "text is redrawn at once")

**Context.** Colours fade from one cover (or theme) to the next (decisions
051, 053): every shape's colour brush animates over `recolorDuration`. But
text, icons and buttons are pixels painted into a surface, and those were
just painted again in the new colours, at once. While they were small
(glyphs, a title) that went unnoticed. The media card's hero play button
(decision 094) is a large circle filled with the cover's colour, painted
the same way, and with every new cover it jumped while the island around
it faded: "the controls' colour change animation is gone".

**Decision.** On a recolour, an element whose pixels sit on a sprite
(`View::surfaceHost`: text, icons, buttons, the dial's time) is painted on
a fresh surface, and the old one fades off over it (`fadeInSurface`, as a
new cover picture does) over `recolorDuration`, the same time the shapes
take. Swimming text (its own marquee brush) and pictures (whose pixels
don't change with colours) are still redrawn in place.

**Consequences.** A recolour briefly holds two surfaces per painted element
(the old one is dropped when its fade ends); fine for a card's worth. Old
over new, so mid-fade nothing is fainter than the new pixels. Needs a
Windows run: a new cover, the play button and toggles fading with the island.
