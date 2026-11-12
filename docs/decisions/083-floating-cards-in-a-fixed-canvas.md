# 083: Floating cards are drawn in a fixed canvas

Status: Superseded by 104: floating cards were dropped for ordinary windows,
so there is no canvas to draw them in. Its lesson stands: a window drawn
with Composition must not show its own moves and resizes as jumps.

**Context.** Decision 081's first Windows version gave every floating card
a window pair of its own: a render window that moves and resizes with the
card, and an input window over it; it flowed out of the pill in a second,
reused layer and handed off to the card's window at the end. On a real
run, Pomodoro's card flickered as it appeared, and its content flickered
and lagged while it was resized. Pomodoro went back to the island's card.

Why it appeared with a flicker:
- **The whole card faded in from nothing**, shape and shadow too
  (`PillRenderer::appear` fades the window's root), while the flow layer
  went `floatHandOffMs` later: the shape all but vanished, then came back.
  Only the content should have faded.
- **The hand-off was timed on the app's clock**, a `DispatcherQueueTimer`
  for the spring's duration. The spring starts at the compositor's next
  commit, so the card's window could show while the copy was still
  settling (or overshooting): it jumped a few pixels. Decision 001 is
  about exactly this: anything that has to meet the compositor is clocked
  from its frames.
- **The reused layer showed a stale frame**: its window was moved and shown
  in the same call that set up the new shape, and Windows showed the
  window a frame before Composition drew the new shape in it.
- **The two shapes didn't match at the hand-off**: two shadows for a
  moment (darker), and the corners and ears eased on `morphEase` over
  `morphDuration`, not on the spring, so they weren't always done.

Why resizing flickered and lagged:
- **Every element whose size changed was rebuilt and faded in**
  (`ElementVisuals::update`: a new size means a new visual). Text and
  progress bars stretch with the card, so each mouse move removed them
  and faded them back. The rule is right for a tokens.json edit, not for
  sixty changes a second.
- **The window and its drawing changed at different times**: the window
  pair moved at once (`DeferWindowPos`), the dispatch's windows pass came
  a queue turn later (`applyWanted`, decision 081), the shape's size the
  frame after, and the content after that (queued at low priority,
  decision 012). Dragging the left or top edge moves the window's corner,
  so the whole card jumped, then caught up.
- **The pipeline animates toward targets; a resize has to follow a hand.**
  Layout, then a spring, then content built later and fading in: right for
  the island morphing, wrong for a live resize, which needs layout and
  drawing in the frame the pointer moved, with no motion.
- **Each mouse move did everything** (a new region, a dispatch, layout,
  text drawn again): more than a frame's worth, so it fell behind.

**The mistake underneath.** The island is smooth because its render window
never moves or resizes (decision 026): one fixed, click-through canvas,
everything moving inside it through the compositor, which keeps every
frame in step; only the invisible input window moves, and its frame of lag
can't be seen (decision 001). Windows doesn't keep a window's own
position and size in step with what Composition draws in it, so a
render window that moves and resizes, and a hand-off between two windows,
show as jumps and flicker. 081's first version broke that one rule. It
came from the first idea, a regular window, and stayed when the idea
became a floating card. The reason given then for not using one canvas
(a topmost window over a whole monitor reads as a fullscreen app, which
holds back notifications) is answered by sizing the canvas to the work
area, not the monitor.

**Decision (proposed).**
- **One fixed canvas per monitor with floating cards on it**: a layered,
  click-through, topmost tool window the size of the monitor's work area
  (the screen less the taskbar, so Windows never takes it for a fullscreen
  app; the fullscreen watcher ignores our own windows), under the island.
  Made when a card first floats on that monitor, kept until the last one
  goes. Floating cards are shapes in it, drawn by the island's renderer
  exactly as the island is: moving and resizing are Composition changes
  only, in step frame by frame.
- **Only the input windows move**, one per card, over the card, its region
  the card's shape, as the island's does (decision 001); a frame of lag in
  an invisible window doesn't show.
- **The flow has no hand-off**: the shape that peels off the pill (in the
  card's monitor's canvas, starting where the pill is) is the card's own
  shape, springing to its place; its content arrives as the island's does
  (decision 080's reveal). Going back is the same shape springing into the
  pill and fading as the pill takes over: the pill never moved, so nothing
  jumps. Corners and ears move on the same spring.
- **A resize mode in the renderer**, while an edge is held: layout and
  drawing in the pointer's frame (the resize comes from a mouse message,
  never inside a dispatch, so it can be applied at once); the shape's size
  set without a spring; elements moved and resized in place, never rebuilt
  for a new size and never faded; text drawn again only when its lines
  change; at most one layout per frame (moves coalesced to the frame clock).
- **A card dragged to another monitor** moves to that monitor's canvas when
  its middle crosses over, and is laid out at that monitor's scale there:
  the per-monitor scale 081 left for later comes with it.

**Consequences.** `platform/float_window` keeps only the input window;
`platform/float_flow` goes (the flow is the card's own shape). The core
(081's rules, placement, limits) and the app's surfaces don't change.
Moving a card is cheaper than now: no window but the invisible one
moves. One more window per monitor with floating cards, drawing nothing
when none is there. The resize mode is new renderer code and needs care
around lists and text fields; it can be judged only on Windows. Until this
lands, nothing floats.
