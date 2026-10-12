# 026: Moving the island along the top edge

Status: Accepted

**Context.** The island always sat in the middle of the top edge. The repo
owner wanted to put it anywhere along that edge, without it ever leaving the
edge, and without dragging getting in the way of clicks.

**Decision.**
- **Press and hold to lift.** A press on the pill (Idle, Compact, Split's
  pill) or an alert that stays within `dragStartDistance` for `liftHold`
  lifts the island. All of it is a handle, buttons included: a lift
  cancels whatever the press started (the button isn't clicked, a scrub
  doesn't seek). A press that moves first is what it always was. Expanded
  never lifts (it's full of controls, scrolling and text), and the Split
  bubble keeps its own drag (decision 007): holding it doesn't lift the island.
- **Lifted:** it sinks `liftDrop` below the screen edge, its flares fold
  and its top corners round (decision 025), and it follows the pointer
  sideways 1:1, never vertically, held on the screen. In Split the bubble
  tucks into the pill (it buds out again on landing). An alert arriving
  morphs it as usual; it stays lifted. Opening Home (a second launch) or
  hiding lands it where it is.
- **Landing** where it's let go, then snapping:
  - within `centreSnap` of the screen's centre: back to dead centre;
  - within `edgeSnap` (never less than `flareRadius`) of a side edge:
    flush with it. In Split the bubble is the right-hand shape;
  - elsewhere: there, kept as a fraction of the screen's width.
  It glides there (the bubble's glide-home, decision 008) and the flares
  grow back, except on a side flush with the screen's edge.
- **The edge rule, in every mode:** a side is flush with the screen edge
  (no flare there: it would hang off the monitor) or at least `flareRadius`
  from it (the whole flare shows), never in between, where the monitor's
  edge would cut a flare in half. `placeAt()` enforces it for each mode's
  width, so a wider shape (an alert, Expanded) near an edge is pushed flush.
  A docked island stays on its edge and grows away from it.
- **Saved per monitor** in `%LOCALAPPDATA%\CupIsland\position.txt`, one
  `<spot>\t<display id>` line each (ids as in decision 023: "" is the main
  display), so a resolution change keeps it in proportion. Settings has a
  **Centre island** button.
- **The render layer spans the monitor's width** (by the canvas's height),
  so moving is Composition alone: no window moves per pointer move. It's
  transparent and click-through, so its size costs nothing visible. While
  lifted the interaction layer stays put (it has capture); on landing it
  jumps to the drop point and follows the glide (decision 001).

**Consequences.**
- Core: `placement.h` (docks, snapping, the edge rule, the file format),
  `lift.h` (the gesture in reduce(), positions for layout()). Covered by
  `tests/scenarios/lift.scenario` and `tests/placement_tests.cpp`.
- `Scene` says `lifted`, `flushLeft`, `flushRight`; the renderer does the rest
  (`Blob::lift`, `Blob::setEars`).
- `canvasSize`'s width is now a minimum for the render layer; its height
  still bounds the tallest card.
- Only the island's own monitor: dragging never crosses to another one
  (Settings → Display does that, decision 023).
