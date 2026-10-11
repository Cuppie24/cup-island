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
  fast gesture, Windows side not built yet), and the pin button in
  Expanded's switcher (the visible one, since a drag is undiscoverable).
- **Expanded switcher:** a host-owned column at the right of the card: "next
  activity" (wraps, in rank order), and "show in the island" unless the
  shown activity already has the pill. Its buttons have no provider id;
  `islandEvent()` turns them into island events.
- **Pick vs. urgency:** the pick sits above every Passive/Active activity,
  but a more urgent (TimeSensitive/Critical) one still takes the pill, as
  it would on an iPhone. A pick made with a quick gesture shouldn't hide a
  meeting starting now. The pick returns to the pill when that ends. This
  departs from PS §6.2's "pin > level", which was written for a pin in
  settings; that settings pin (per provider) is still to come.
- **Lifetime:** one pick at a time; it's cleared when its activity ends or
  expires, so a restarted activity doesn't inherit a stale pick.

**Consequences.** All of it lives in core and is covered by
`tests/scenarios/switching.scenario`. Flipping the urgency rule is the one
line in `priority()` (`core/island.cpp`). The Expanded center slot is a
button narrower when the switcher shows, so providers' text wraps sooner.
