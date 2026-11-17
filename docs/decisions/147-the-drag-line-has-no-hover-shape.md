# 147: The drag line has no hover background

Status: Accepted (amends 139). The look on Windows needs a run.

**Context.** A card's drag line was a button lit as a 64 × 20 pill under the
pointer (decision 139), so it read as the one place to grab. Since decision
143 the whole header drags the card, with the grab cursor saying so; the
pill only drew the eye to a smaller target. The user asked for it to go.

**Decision.** `Placed::noHoverShape`: a button drawn with no shape under the
pointer. The drag line sets it; its line still shows while the pointer is on
the card, and still picks the card up, as the rest of the header does.
