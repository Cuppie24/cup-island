# 046: A pin keeps the island open

Status: Accepted

**Context.** Expanded closes by itself after `expandedIdleTimeoutMs`, and on
any click outside the island. Reading a card, or keeping the to-dos up next
to other work, meant reopening it over and over.

**Decision.**
- **A pin at the tab row's right end**, mirroring Home's button at the left
  (same size, its click area reaching into the card's bottom-right corner).
  The row already kept that room free so the tabs stay centred; it's now
  the pin's. The pin shows Pin, or a filled pin lit like the active tab
  while on. Keyboard focus reaches it; Enter or Space toggles it.
  `TabStrip::pin`, not one of `tabs`, so Ctrl+Tab never lands on it.
- **Pinned (`IslandState::stayOpen`):** no idle timeout, and neither a
  click outside the island nor one on a card's empty space closes it.
  Esc, the card going away, a critical alert, going Hidden (fullscreen) still
  close it, and closing unpins it: the next time it opens, it's
  unpinned. It stays pinned across tabs and Home. Unpinning restarts the
  idle timeout from that moment.
- **Home always has its tab row now**, even with nothing running (Home's
  button and the pin), so the pin is always there with panels. A lone card
  with no panels and no other activity still has no row (tests only: the
  app always has panels).
- **Taller tabs:** `tabHeight` 28 → 36, and an activity tab's click and
  hover area runs from half the gap above it to the card's bottom edge and
  half the tab gap to each side, like Home's button.

**Consequences.** Rules in `island.cpp`, tested by `tests/scenarios/pin.scenario`;
the strip in `tabs.cpp` (content_tests, home_tests). The lit pin is a second
highlight in `Blob` (`setPinnedTab`); drawing it needs a Windows run.
