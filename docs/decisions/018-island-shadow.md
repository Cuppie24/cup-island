# 018: The island's shadow

Status: Accepted

**Context.** The island should cast a shadow in every mode (compact, the
Split bubble, Expanded), so it lifts off whatever is under it, light
wallpapers and windows especially.

**Decision.** Each shape (the pill, the bubble) gets a shadow caster under
it: a `SpriteVisual` with no brush, so it draws nothing itself, carrying a
`DropShadow` whose mask is a live `CompositionVisualSurface` snapshot of
the shape. So:
- the shadow has the shape's outline (rounded bottom), not its rectangle;
- the caster's size, offset, scale and pivot are bound to the shape's by
  expressions, and the snapshot re-renders on the compositor, so it
  follows every morph, drag and press squash with no app code, and an idle
  island still costs nothing (TR §8);
- Hidden (height 0) casts nothing.
- The liquid neck casts none: it's thin and short-lived.

`CreateVisualSurface` is Windows 10 1903+. It's looked up at runtime
(decision 015); without it there's no shadow and nothing else changes.

Tokens: `shadowColor` (its alpha is the strength), `shadowBlur`,
`shadowOffset` (down, since the island hangs from the top edge). Like every
length, blur and offset follow the display's scale and `islandScale`. The
canvas grew to 440×420 so the blur isn't cut off at the sides or under the
tallest card.

(Tried first: pill, bubble and neck in one `LayerVisual` with a
`DropShadow`, for a single shadow of the combined shape. On a real build it
cast a shadow of the whole canvas rectangle, and the island inside the
layer wasn't drawn at all. Shapes stay where they always were now; the
shadow is extra visuals under them, so a shadow problem can't hide the
island.)

**Consequences.** `Blob` builds and updates its own caster
(`buildShadow`, `applyShadow`); nothing else changes. Needs a Windows run
to judge the look.
