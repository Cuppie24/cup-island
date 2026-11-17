# 133: The snapshot's shapes are antialiased (BorderMode Soft)

Status: Accepted; confirmed on Windows 11 in 0.12.6, after two releases that didn't fix it (amends 018, 089, 097)

**Context.** On a light desktop the island's rounded corners looked
pixelated, much more so with glass on. A screenshot showed why: along the
curve the outermost pixels jump straight from a grey band (~75) to the
desktop (~230), with almost no values between. The tint over the glass (the
twin's layer, decision 097) is antialiased; the glass under it is cut by the
twin's snapshot (`CompositionVisualSurface`), and that cut was hard: each edge
pixel all glass or none. Where the tint covers a pixel partly, the glass
under it still fills it, so the hard cut shows through as a staircase.
Decision 097's "dark fringe stepping like jaggies" was likely the same.

**Tried first: blurring the mask** (0.12.3, 0.12.5). Windows refused a
Gaussian blur effect at the factory (`0x80004002`) with every setting we
tried (Quality, Balanced, Speed, the amount alone), so it was removed.

**Decision.** The cause is the visual's **border mode**: a visual's
`BorderMode` Inherit takes its parent's, and the snapshot draws the twin as
its own root, where Inherit resolves to Hard (aliased edges). Lottie-Windows
hit the same with its mask trees. The twin and every ear's ShapeVisual are
set to **Soft**, so the snapshot, and the glass's cut and the shadow made
from it, have antialiased edges like the tint over them. `BorderMode` is
Windows 10 1511, within decision 015.

**Confirmed.** On Windows 11 at 100% scaling, glass on, over a white
window, the corners are smooth. Before, they stepped: the bug was first
reported on 0.12.2 and was still there in 0.12.3 and 0.12.5.

**The rule this leaves:** any visual tree drawn into a
`CompositionVisualSurface` (a snapshot used as a mask, a shadow or a
brush) sets `BorderMode` Soft on its root, and on any child that sets its
own. Composition gives no warning. The tree looks fine on screen and is
aliased only in the surface, so the bug shows only where the surface is
used (here, through a see-through tint). The ear's ShapeVisual sets Soft
itself because `addEars` builds it for the body as well as the twin.

**How it was found.** Pixel values along the curve in a screenshot
(`python -c` with PIL over a 1:1 capture): an AA edge has a run of in-between
values; this one jumped from ~75 to ~230 in one pixel, and only under glass.
The log's `glass:` lines then showed the blur attempts refused, which ruled
that path out. Read the corner's pixels before guessing again.

**Consequences.** The shadow's mask is smoother too (unseen: it's blurred).
`glassEdgeSoftness` existed only in 0.12.3–0.12.5. The installer replaces
tokens.json, but a hand-edited copy that still has it is rejected as a
whole ("unknown token (typo?)") and the previous values stay: delete the line.
