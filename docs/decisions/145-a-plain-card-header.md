# 145: A plain card header lights nothing, its buttons at its far end

Status: Accepted (amends 123, 142). Core tested; the look on Windows needs a run.

**Context.** Decision 123 made a card's title row a list item that hugs its
name, with what's at its end (a delete) right beside it, lit that far under
the pointer: it was the field the name was edited in. Once a group's name
stopped being edited in place (decision 142), the row still lit as one pill
round the name and its "…", and the "…" sat right after the name. The user
wanted nothing on the name to react to the pointer and the "…" at the
header's end.

**Decision.** A card whose name isn't edited in place (`titleEdit` empty) has
a plain title row (`Row::inset`): its name in line with the rows' text,
nothing lit under the pointer, and its trailing items (the "…") at the row's
far end; the "…" lights alone, as a button. A card whose name is edited in
place keeps decision 123's row.
