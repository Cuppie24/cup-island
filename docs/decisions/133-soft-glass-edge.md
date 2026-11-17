# 133: The snapshot's shapes are antialiased (BorderMode Soft)

Status: Accepted; built, not yet run on Windows (amends 018, 089, 097)

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

**Consequences.** The shadow's mask is smoother too (unseen: it's blurred).
Needs a Windows run: the corners on a white desktop, glass on and off.
