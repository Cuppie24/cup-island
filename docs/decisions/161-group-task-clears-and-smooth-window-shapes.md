# 161: A group's menu clears its tasks; the window's rounded shapes are antialiased

Status: Accepted (amends 133, 142). Core tested; the corners need a Windows run.

**Context.** Two requests from the timer window. A group's "…" had only
Rename and Delete; clearing a group's solved tasks, or all of them, meant
deleting them one by one. And the window's question (decision 122) looked
pixelated: a screenshot's pixels along the panel's corners jumped straight
from the panel (~228) to the scrim (~135), with no values between, the
same staircase 133 found on the island.

**Decision.**
- **Delete solved tasks / Delete all tasks** sit in a group's menu between
  Rename and Delete, each in its own menu group (thin lines between). They
  send the to-do list's own clears, in the group (`inGroup(id,
  todo_action::kClearDone / kClearAll)`); the provider asks first over the
  window ("Delete solved tasks in “Work”?", "2 solved tasks will be
  deleted."), and the confirm sends the list's confirmed clear. Confirmed,
  they're gone with no Undo, as a confirmed group delete is (122). Each is
  dim while it has nothing to clear.
- **`roundedRect` in a provider's window sets `BorderMode` Soft.** Its
  ShapeVisual inherited Hard from the window's tree, so every rounded
  shape there (the question's panel, the picked page's highlight, the lit
  row) had aliased corners; the panel's 28px radius showed it most. Only
  the rounded shapes take Soft: the window's straight rectangles sit on
  whole pixels, and Soft edges at a fractional size would leave faint seams.

**Consequences.** If other window shapes still step (a GroupFill, a
button's fill), they're built elsewhere (`element_parts`) and need the same
line. Read the edge's pixels in a 1:1 screenshot before guessing (133).
