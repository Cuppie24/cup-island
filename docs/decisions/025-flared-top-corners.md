# 025: Flared top corners

Status: Accepted

**Context.** The island hangs from the top edge with a square top and
rounded bottom corners, so it read as a shape stuck under the screen edge.
A "reversed" (concave) radius at the top corners makes it look like it
grows out of the edge, as notches do. The repo owner wanted it on the pill
and the Split bubble, subtle by default.

**Decision.**
- **Two "ears" per shape** (`platform/blob_shape.cpp`): small island-coloured
  pieces beside the top edge, as children of the body. Being children, they
  move, morph, squash on press and follow a drag with it, for free.
  - Each ear is an E×E square minus a quarter circle of radius E centred
    on its bottom outer corner.
  - Composition has no concave corners. A path (`CompositionPathGeometry`)
    would need Direct2D interop (`IGeometrySource2DInterop`), so each ear
    is plain shapes instead: a ring stroke around that corner whose inner
    edge is the curve, thick enough to cover the square, clipped to the
    square by its ShapeVisual.
  - The ears share the body's fill brush, so themes and the drop-target
    highlight (decision 007) colour them too.
  - The pill's own geometry, corner expressions and morph are untouched.
- **E = `flareRadius`, capped by the shape's height,** times an
  `Attached` factor (1 at the screen edge). It's an expression over the body's live size, like the bottom
  corners (decision 006), so it holds mid-morph. A Hidden pill (height 0)
  and the tucked bubble have none. `flareRadius` 0 turns them off.
- **A dragged bubble becomes a circle** (`Attached` animates to 0): away
  from the screen edge there is nothing to flare into, and a straight top
  makes no sense in the air. Its ears fold away, and the same factor
  rounds its top corners: the background is no longer made taller and
  shifted up, so every corner has the bottom's radius, half the height.
  The bubble is square, so that's a circle. The shadow follows, being a
  snapshot. All of it reverses as it glides home.
- **The shadow includes them** (decision 018): the snapshot and its caster
  are E wider on each side, so the shadow follows the curve.

**Consequences.**
- At rest in Split, the pill's right ear and the bubble's left ear meet
  across the gap (`bubbleGap` ≈ `flareRadius`), so the edge pours into
  both. That's intended.
- The ears are decoration: hit areas stay the pill's rectangle.
- The ears stick out by E on each side, so the widest pill plus 2 E must
  fit the canvas (a unit test checks it).
- Ellipse geometry, shape strokes and ShapeVisual are Windows 10 1803
  APIs, within decision 015. Before 1903 there's no shadow, as before.
