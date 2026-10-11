# 006: Switching which activity has the pill

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
  a drag. The bubble lifts and follows the cursor 1:1. Once its centre is
  over the pill, the island *previews* the swap before the button is let
  go: the dragged shape grows to the pill's size under the cursor and shows
  its compact content, while the old pill shrinks into the bubble. Moving
  off undoes the preview; letting go commits it (the shape lands in the
  pill) or springs the bubble home. Losing capture or an alert cancels.
- **Expanded switcher:** a host-owned column at the right of the card: "next
  activity" (wraps, in rank order), and "show in the island" unless the
  shown activity already has the pill. Its buttons have no provider id;
  `islandEvent()` turns them into island events.
- **The pick beats every level**, as PS §6.2's "pin > level" says: it stays
  in the pill until the user switches again. Urgent things still get through
  as alerts, which are overlays and don't depend on ranking.
- **Lifetime:** one pick at a time; it's cleared when its activity ends or
  expires, so a restarted activity doesn't inherit a stale pick.

- **Shapes follow activities, not roles.** The pill and the bubble are two
  identical `Blob`s; when the activities they show trade places, so do the
  shapes. That one rule gives the swap, the bubble budding out of the pill
  (Compact → Split), and a clicked bubble growing into the Expanded card.
- **Liquid neck:** a black strand between the two shapes' centres, thick
  when their edges touch and gone at `bridgeReach` apart (less than
  `bubbleGap`, so none at rest). Pure expressions over the shapes' live
  values, so it costs nothing per frame. A strand with round ends, not a
  true metaball outline: close enough at this size, and no per-frame path.

**Consequences.** The rules live in core and are covered by
`tests/scenarios/switching.scenario` and `tests/content_tests.cpp`. The
renderer only follows `Scene.drag` and the shapes' owners. The interaction
layer covers the pill plus the bubble (`interactiveBounds`), so the gap
between them also takes clicks (and ignores them). The Expanded center slot is a
button narrower when the switcher shows, so providers' text wraps sooner.
