# 135: A dragged card lands by the pointer, over the cards as shown

Status: Accepted (amends 129). Core tested; the feel on Windows needs a run.

**Context.** Decision 129 placed a dragged card by its own middle, measured
against the cards as laid out before the drag. Dragging a tall group under a
small one: the small one moved up, but in the room the tall one left the
pointer would sometimes send it back down. The tall card's middle sits far
below the pointer, and the small card's old place isn't where it shows, so
the swap happened nowhere near what the user was looking at.

**Decision.** `cardTarget` works on the page as last shown (the preview,
the others already making room) and on the pointer, not the card's middle.
Over another card: before it on its first half (its top; its left when cards
sit side by side on that row), after it on its second. Over no card: the
target stays as it was. So the small card goes back down only when the
pointer is over its top half where it shows. Rows keep decision 129's rule:
a card's rows are close together, with no room between them to wander in.
