# 105: Every edge snaps to its pixel the same way

Status: Accepted (refines decision 024's pixel snapping)

**Context.** Decision 024 puts element visuals on whole pixels: an edge
rounded to the nearest one, so text isn't resampled. Sizes are fractional
at every display scale, because `islandScale` (0.85) multiplies them, so
edges land between pixels all the time. Settings' theme page in its
window (decision 104) showed a line across its Dark group's background
(decision 090), always between Lagoon and Ember, moving with the rows,
there from the first frame, glass or not.

Each of a group's rows has its own piece of the background, and core lays
them out to meet: a piece ends where the next row starts. But the one
edge was computed two ways. Lagoon's piece ended at its top plus its
height, 243.5; Ember's began at the next row's top, 243.49998. Equal but
for float noise, they rounded to different pixels (244 and 243): the two
translucent pieces overlapped by one, and the fill showed twice there.
Every other join on that page happened to round alike, so it was this one.

Three fixes were tried:
1. Rebuild an element whose snapped size changes as it moves (#136). A
   real case, but not this one: the pieces were new, not moved. It stays.
2. Snap edges to 1/64 px before their whole pixel (#137). Right, but only
   `snapToPixels` used it, which sets a visual's size; its place was still
   `std::round`. Ember's piece was placed at 243 and sized from 244: still
   overlapping Lagoon's, and now a pixel short of Lime's, a dark line too.
3. Place visuals through the same snapping (#138). Both lines went.
Making the layout's sums agree instead (a piece ending at the next row's
top, by the same expression) was tried and dropped: in a scrolling list
the pieces' heights then changed by float noise as it scrolled, and were
rebuilt, which is the flicker `group_tests` guards against.

**Decision.**
- **One rule for every edge:** `snapEdge` (core/geometry.h) takes a value
  to the nearest 1/64 px, then to the nearest whole pixel. Values that
  differ only by float noise land on the same pixel; real differences
  (layout values are multiples of scaled tokens, far coarser than 1/64)
  are unchanged.
- **Places and sizes both use it:** `snapToPixels` for a visual's size (its
  edges), `pixelOffset` in element_visuals.cpp for its place. A visual's
  place and size come from the same two edges, so neighbours that core
  makes meet, meet on screen.
- Anything new that puts content on whole pixels goes through `snapEdge`,
  never `std::round` on its own: two roundings of one edge are how this
  happened.

**Consequences.** Group backgrounds join without a line at any scale. The
test (`groupPiecesMeet`, tests/settings_new_tests.cpp) lays out the theme
page in its window at the app's real scale and checks each piece ends on
the pixel the next begins on; it fails without `snapEdge`. Where a visual
is placed (`pixelOffset`) is platform code, judged on Windows only.
