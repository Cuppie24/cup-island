# 130: A button lights only over its shape

Status: Accepted. Core tested; the feel on Windows needs a run.

**Context.** On the timer's island card, the expand button in its corner lit
up with the pointer well away from it: a button's click area (`hitFrame`)
reaches into the gaps round it (and a corner button's to the card's edge),
so it's easy to hit, and hovering used that same area while its hover shape
is only its frame.

**Decision.** What's under the pointer lights only where it's drawn
(`lightsAt`, core/hit_test): a button over its own shape (its frame); one lit
by its row (`Placed::litByRow`, a clickable row's) and anything else wherever
it takes a click. A click still takes all of a button's area. On the island
and in windows alike.
