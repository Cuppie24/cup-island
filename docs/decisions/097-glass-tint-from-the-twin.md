# 097: As glass, the island's tint is the twin's layer, over the glass

Status: Accepted; built, not yet run on Windows (amends 089)

**Context.** Glass (decision 089) stacked, bottom to top: the outline twin
(decision 018: the body's shape, opaque, snapshotted as the mask of the
shadow and of the glass), the glass, then the body with its fill made
see-through (`islandFill`). Three problems came out of that on screen, all
along edges where a pixel is only partly inside the shape:
1. **A dark fringe round the bottom corners.** The glass covered the opaque
   twin only partly there, so the twin showed through beside the rim's
   light line, stepping like jaggies on light desktops.
2. **A light line between the island and its flared corners while it
   moved** (most of all while held, as the press shrinks it). As glass the
   body's ears only meet it (`shape.Overlap` 0: overlapping, two
   see-through fills tint the overlap twice, a darker line). At rest on
   whole pixels that's clean; between pixels the seam pixel is covered
   partly by each side and gets less tint.
3. The twin's ears had the same seam, so the glass's mask had it too.

Tried on the way, and why they weren't enough: hiding the twin (fixed 1,
exposed 2); showing it again while the shape changed (its seam was the
same); the twin's ears overlapping (fixed 3, not 2); the press's scale on
the twin's parent (the snapshot then can't scale the glass's cut twice,
whatever Windows does with a source's own scale; kept, but 2 remained).

**Decision.** As glass, the **twin is the tint**, over the glass:
- The twin sits in a **LayerVisual** (sized from the parent's origin past
  the body's far corner), made see-through as one piece by the layer's
  opacity: `glassShare` (the Transparency pick's opacity, plain or evened
  blur; 1 when solid). Its ears always overlap the body by 1 px
  (`addEars`' `overlap`); in one layer the overlap is drawn once, so the
  shape is seamless at any position.
- The **glass goes under** the layer. The body's own fill is **clear**; its
  glow, rim and content are over the layer as before.
- Nothing opaque is under the glass any more, so no dark fringe (1), and one
  seamless fill (2), cut from a seamless mask (3).
- The layer carries the press's scale (about `pill.Offset +
  pill.CenterPoint`, the body's centre point in the parent's space); the
  snapshot draws the twin as its own root, without the layer's opacity or
  scale (it ignores even the twin's own `Offset`), so the shadow and the
  glass keep the full shape.
- Glass on: the glass is there at once under the still-opaque layer, which
  fades to its tint as the body's fill fades to clear. Off: the reverse.
  A Transparency pick fades the layer.
- Solid, the layer is opaque under the opaque body, as the twin always was.
  Without a glass (Windows refused the brush) it stays as 089 had it: the
  body see-through over the opaque twin.

**Consequences.** A layer is an offscreen surface the size of the body's
reach, re-drawn only when the twin changes (the shape); compositing it is
cheap. `glassShare` is tested in `tests/tokens_tests.cpp`; the rest needs a
Windows run: the corners at rest on a light desktop, and holding and
dragging the pill.
