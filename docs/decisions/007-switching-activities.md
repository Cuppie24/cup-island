# 007: Switching which activity has the pill

Status: Accepted

**Context.** With several activities live, PS §6.2 ranks them automatically
and promises a "user pin", but gives no way to set one from the island, and
doesn't say what clicking the Split bubble does. Users want to look at the
second activity, and sometimes to make it the main one.

**Decision.**
- **Bubble click** expands the bubble's activity. Looking never re-ranks.
- **Promote** (`ActivityPromoted`) makes an activity the user's pick
  (`pinnedId`). Two ways to send it: dragging the bubble onto the pill (the
  fast gesture), and a tab in Expanded (the visible one, since a
  drag is undiscoverable).
- **Expanded tabs:** with several activities, a strip of tabs runs along
  the top of the Expanded card (moved to the bottom, with a Home button
  apart, by decision 009), one per activity (the shown one included),
  and the card grows by the strip's height. Each tab: the compact leading
  item (icon or art) and a title (the compact centre text, else the
  activity's name; ends in "…", no swimming: a row of moving titles is
  busy). Built from what providers already give for Compact, so they need
  nothing new. Tabs keep the order activities started in, so clicking one
  never reorders them. Every tab is `tabMaxWidth` wide and the group is
  centred, shrinking only when they don't fit. Inside a tab, icon and title
  are centred as one group; that needs the title's real width, so
  `layout()` takes a `MeasureText` the platform answers with DirectWrite
  (tests use a per-character estimate). The active tab has a soft
  background that slides to a newly picked tab; others light up faintly on
  hover. Clicking a tab (`ActivitySwitched`) shows that activity's card *and*
  makes it the pick, so collapsing shows it in the pill.
  (Tried before: "next"/"pin" buttons in the top row, hard to read and
  crowding the title; then a detached side card listing the others, which
  needed a 720-wide canvas and split the island in two.)
- **Drag-to-swap:** a press on the bubble that moves `dragStartDistance` is
  a drag, and the bubble follows the cursor 1:1. Nothing else moves. Once its
  centre is over the pill, the pill's background lightens (`islandHighlightColor`):
  letting go there swaps the two activities' content and the bubble slides
  home; letting go elsewhere just sends it home. Losing capture or an alert
  cancels. (A first version previewed the swap by trading the shapes'
  sizes mid-drag; it was too much motion.)
- **Fixed roles:** the pill stays the pill and the bubble the bubble. A
  clicked bubble tucks away while the pill grows into its activity's
  Expanded card, like any other expand.
- **Same shape:** the bubble hangs from the top edge like the pill
  (straight top, rounded bottom), and its content uses the same nested
  radius (decision 006), so art sits in it the way it sits in the pill.
- **Liquid neck:** a black strand between the two shapes' centres, thick
  when their edges touch and gone at `bridgeReach` apart (less than
  `bubbleGap`, so none at rest). It shows when the bubble buds out of the
  pill or settles back next to it; it's off while dragging. Pure
  expressions over the shapes' live values, so it costs nothing per frame.

**Consequences.** The rules live in core and are covered by
`tests/scenarios/switching.scenario` and `tests/content_tests.cpp`. The
renderer only follows `BubbleScene.dragCentre` and `Scene.dropTarget`. The interaction
layer covers the pill plus the bubble (`interactiveBounds`), so the gap
between them also takes clicks (and ignores them). The canvas is 220 tall
for Expanded plus its tab strip.

**Open.** The settings pin per provider (PS §6.2) isn't built; the pick is
the only pin. The only second source today is the demo timer (T key). The
user prefers calm motion: the first drag version (shapes trading sizes,
lift, neck while dragging) was rejected as too busy. Composition handoffs
behind the drag are in decision 008.
