# 157: A lone group stays put while dragged

Status: Accepted. Core tested; the look needs a Windows run.

**Context.** With one group on the Timer page, dragging it moved its drop
outline after "New group": "New group" slid to the start and the outline sat
where it had been. `withMove` places a card among the other cards that move;
with none, it fell back to the end of the page, past the cards that don't.

**Decision.** With no other movable card to place it by, the carried card
keeps the place it had. Dropped there, nothing changes (the target is still
its origin), as before.

**Consequences.** `tests/window_drag_tests.cpp`.
