# 170: A toast's edge is its own; its shadow falls below

Status: Accepted (amends 151). The look needs a Windows run.

**Context.** In a window, the toast's border looked blurred, and roughly
so: no clean shape, a dark haze all round it. 151 put the pill in a
`LayerVisual` so the layer's `DropShadow` took its shape. That draws the
pill into an offscreen layer first: its edge came out of the layer, not
from the shape (stepped, under a window's Hard border mode, decision 161),
the outer half of its stroke was cut at the layer's bounds, and the shadow
(blur 14, offset 3) reached almost as far above the pill as below.
Decision 018 had already found a `LayerVisual` with a shadow unreliable.

**Decision.**
- **The pill is a `ShapeVisual` straight in the tree**, as the menu's panel
  is: `BorderMode` Soft (161), its stroke inset by half its thickness so
  none of it is cut. Its edge is the shape's own, antialiased and sharp.
- **The shadow is a caster under it**, as raised art's (094): a sprite with
  no brush, its `DropShadow` masked by a surface painted with the pill's
  rounded shape (`paintShadowMask`; repainted when the pill's size changes
  or the device was rebuilt, decision 164). `DropShadow.Mask` is Windows 10
  1607 (decision 015). A shadow problem can no longer blur the pill.
- **It falls below**: `toastShadowBlur` 10, `toastShadowOffset` [0, 6], so
  little of it shows above the pill or at its sides.

**Consequences.** The island's cards get the same toast. The shadow still
slides, fades and is clipped with the toast at the page's bottom edge.
