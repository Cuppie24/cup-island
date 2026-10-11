# 018: One shadow for the whole island

Status: Accepted

**Context.** The island should cast a shadow in every mode (compact, the
Split bubble, Expanded), so it lifts off whatever is under it, light
wallpapers and windows especially.

**Decision.** The pill, the bubble and the liquid neck between them are
built into one Composition `LayerVisual` covering the canvas, and that
layer has a `DropShadow`. The shadow comes from the layer's combined alpha,
so:
- it's one shadow for one shape: where the bubble buds off and the neck
  stretches, the shadow does the same, without two shadows overlapping;
- it follows every morph, drag and press squash with no code: it's all on
  the compositor, and an idle island still costs nothing (TR §8);
- Hidden (height 0) casts nothing.

Tokens: `shadowColor` (its alpha is the strength), `shadowBlur`,
`shadowOffset` (down, since the island hangs from the top edge). Like every
length, blur and offset follow the display's scale and `islandScale`. The
canvas grew to 440×420 so the blur isn't cut off at the sides or under the
tallest card.

(Not chosen: a shadow per shape. A ShapeVisual can't carry one anyway, so
it would mean a SpriteVisual mask per blob, and two shadows would overlap
where the neck joins them.)

**Consequences.** `platform/pill_renderer` owns the layer and its shadow;
`Blob` and `LiquidBridge` are unchanged, they just build into the layer.
Needs a Windows run to judge the look.
