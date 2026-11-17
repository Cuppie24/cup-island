# 136: A dragged task lands by the pointer too

Status: Accepted (extends 135 to rows). Core tested; the feel on Windows needs a run.

**Context.** Decision 135 placed a dragged card by the pointer over the cards
as shown. Tasks still used decision 129's rule (the carried row's middle,
against the page as laid out before the drag), so a tall, wrapped task
behaved as a tall group did: the room it left could send a shorter task back.
The user asked for tasks to work the same way as groups.

**Decision.** One `dropTarget(shown, drag, current)` for both, on the page
as last shown and the pointer. A row, in the card under the pointer: over
another row, before it on its top half, after it on its bottom half; above
the card's rows (its name, "Add task"), first; below them, last; in the gap
the carried row leaves, as it was; in a gap in another card, between the
rows it's between. Over no card: as it was, as a card is (it used to go back
where it started). Let go outside the window, it still goes back.

A card's "side by side" check now looks at every card shown on its row (the
"New group" card, the drop outline), not only the ones that move: with two
groups side by side, there was no other movable card to see.
