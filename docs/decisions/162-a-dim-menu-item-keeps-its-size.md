# 162: A dim menu item keeps its size

Status: Accepted (amends 086).

**Context.** A menu's disabled items were drawn in Caption style: smaller
than the enabled ones as well as greyer. In the timer window's group menu
(decision 161), an empty group's "Delete solved tasks" and "Delete all
tasks" read as a different kind of row, not as the same rows switched off.

**Decision.** Every menu label is Body. A disabled item's label (and its
shortcut) is `Placed::muted`: grey and dim, as an inactive tab's items are
(decision 072). Only the colour says it's off. Its width is measured in
Body too, so the menu doesn't widen when an item is enabled.

**Consequences.** One look for disabled items on the island's menus and the
windows' (both draw through `MenuLayer`). If muted reads too faint, it's the
tab's grey: tune it there, for both.
