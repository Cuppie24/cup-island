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
  fast gesture), and a row in Expanded's side card (the visible one, since a
  drag is undiscoverable).
- **Expanded side card:** with several activities, the others are listed in
  a second, narrower card a `bubbleGap` right of the Expanded card, hanging
  from the top edge like it. It's the Split bubble's shape grown into a list,
  so expanding from Split reads as one object getting bigger. One row per
  activity, in rank order: the compact leading item (icon or art), a title
  (the compact centre text, else the activity's name; swims if long) and a
  short status (compact trailing text like "4:59", or a glyph such as the
  equalizer). Built from what providers already give for Compact, so they
  need nothing new. No actions in rows: a row is one click target. Clicking
  it (`ActivitySwitched`) shows that activity's card *and* makes it the pick,
  so collapsing shows it in the pill. More activities than `sideCardRows`:
  the last row reads "+N more" and opens the first of them. Rows light up as
  a whole on hover; the content crossfades on a switch, nothing slides.
  (This replaced a first try: "next" and "pin" buttons in the card's top
  row, which were hard to read and crowded the title.)
- **Drag-to-swap:** a press on the bubble that moves `dragStartDistance` is
  a drag, and the bubble follows the cursor 1:1. Nothing else moves. Once its
  centre is over the pill, the pill's background lightens (`dropHighlight`):
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
between them also takes clicks (and ignores them). In Expanded it covers
the side card too, which is why the canvas is 720 wide: the card stays
centred and the side card needs room on its right.

**Open.** The settings pin per provider (PS §6.2) isn't built; the pick is
the only pin. The only second source today is the demo timer (T key). The
user prefers calm motion: the first drag version (shapes trading sizes,
lift, neck while dragging) was rejected as too busy. Composition handoffs
behind the drag are in decision 008.
