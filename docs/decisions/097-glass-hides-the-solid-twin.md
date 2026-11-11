# 097: As glass, the solid twin under it is hidden

Status: Accepted; built, not yet run on Windows (amends 089)

**Context.** Under the glass (decision 089) sits the outline twin
(decision 018): the body's shape, opaque, in the island's colour, there to
be snapshotted as the mask of the shadow and of the glass. Inside the shape
the glass covers it. Along a curved edge it doesn't, quite: a pixel there
is only partly inside the shape, so the twin, the glass and the body each
cover it partly, and the dark twin shows through as a fringe. On a light
desktop that fringe beside the rim's light line stepped round the bottom
corners like jaggies. Straight edges have no part-covered pixels, which is
why only the corners showed it.

**Decision.** While glass is on, the twin is hidden. It's in a holder of its
own (a ContainerVisual) and the **holder's** opacity drops, not the twin's:
the snapshot (`CompositionVisualSurface`) draws its source visual as its own
root, without its parents (it already ignores the twin's place, which is why
its `SourceOffset` needs no `pill.Offset`), so the shadow and the glass keep
the shape. The opacity is 0.01, not 0, so Windows never skips drawing the
subtree as invisible; at that strength the fringe is gone. Off, the holder
is opaque again at once, under the glass fading out. The glass sprite is
inserted above the holder, not inside it.

**The twin's ears always overlap the body.** As glass the body's ears
only meet it (`shape.Overlap` 0, decision 089: a see-through overlap draws
a darker line), and so did the twin's. Mid-motion (a drag, a morph's
spring) the island sits between pixels, so the pixel at that seam was only
partly covered by each side, and the twin, being the glass's mask, cut the
glass there too: a thin line of the unblurred desktop between the island
and its corners while it moved, gone at rest (whole pixels). It had been
there since glass came in. The twin's ears now reach 1 px into it always
(`addEars`' `overlap`): it's opaque, so overlap draws nothing darker, and
the glass behind the seam is whole. The body's own seam is left: over
whole glass it's at most a slightly fainter tint. (Showing the twin while
the shape changed was tried first, on the guess that the snapshot lagged;
it didn't help, since the twin had the same seam, and it was taken out.)

**The twin's scale is its holder's.** The gap at the corners showed only
while the pill was held, before it lifts to be dragged: the press shrinks
the body (`pressScale`, about its top centre). The twin was scaled the
same way, and so are the glass and the shadow's caster, which stretch the
snapshot over themselves. If the snapshot draws the twin with its own
scale, the glass's cut shrank twice and stopped short of the corners; the
opaque twin, visible before, hid that. Now the holder carries the scale
(about `pill.Offset + pill.CenterPoint`, the body's centre in the parent's
space), so the twin on screen still shrinks with the body, while its
snapshot, drawn without its parents (as its ignored `Offset` already
shows), is always full size and is scaled once, by the glass. The seam fix
above didn't cure this gap but is kept: the seam is real at fractional
positions.

**Consequences.** If the snapshot did take its parent's opacity after all,
glass and its shadow would vanish while glass is on: the first Windows run
says. Solid islands are unchanged.
