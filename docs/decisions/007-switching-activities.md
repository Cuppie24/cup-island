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
  fast gesture), and the pin button in Expanded's switcher (the visible one,
  since a drag is undiscoverable).
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
between them also takes clicks (and ignores them). The Expanded center slot is a
button narrower when the switcher shows, so providers' text wraps sooner.
