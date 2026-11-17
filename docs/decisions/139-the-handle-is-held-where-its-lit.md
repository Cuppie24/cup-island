# 139: A card's handle is held where it's lit

Status: Accepted (amends 134). Core tested; the feel on Windows needs a run.

**Context.** Decision 134 drew a group's handle as one line in a small box
(`smallButtonSize` × 12), lit under the pointer, and took presses in a wider
band around it (48 × 16). What the user saw lit was the small box, so the
handle looked, and felt, too small to grab; asked earlier to make the band
"only a little bigger", it had become hard to hit.

**Decision.** One box: `windowDragHandleWidth` × `windowDragHandleHeight`
(64 × 20), `windowDragHandleTop` (2) under the card's top, is both what's lit
under the pointer and what takes a press (`frame == hitFrame`); the line
(`windowDragLineWidth` × `windowDragLineThickness`) is drawn in its middle.
`windowDragHitWidth` and `windowDragHitHeight` are gone. Where the box
reaches over the group's name, a press that never moves is still the name's
click (decision 134).
