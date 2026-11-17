# 127: A task row being written is added when it's left

Status: Accepted (amends 050 for this one kind of field: its text isn't kept for later). Core tested.

**Context.** Clicked, "+ Add task" is the task it adds, being written
(`TextInput::focusedIcon`, decision 123). Typed into and then left with a
click elsewhere, its text was kept in the field for later (decision 050), as
any field's is, so no task appeared: the user had to press Enter.

**Decision.** A field that is a row being written (`Draft::sendsOnLeave`,
set when such a field is focused) sends its text when it's left, as Enter
does: a click elsewhere, another field focused. Blank (spaces only), it
sends nothing and keeps nothing. Esc still drops it: Esc cancels.
Other fields keep their text as before (decision 050).
