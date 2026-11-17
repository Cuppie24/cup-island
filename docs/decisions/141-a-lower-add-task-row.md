# 141: A lower "Add task" row

Status: Accepted (amends 132). Core tested; the look on Windows needs a run.

**Context.** "+ Add task" (decision 132) is a whole-row button as tall as a
task's row (`smallButtonSize`, 32), on every group: the user found it took
too much room. Offered a lower row, a "+" in the group's header, or a row
shown only on hover, they chose the lower row, keeping its words.

**Decision.** `Button::low`: a compact button `lowButtonHeight` (24) tall,
its width still `smallButtonSize`, so its + stays over the tasks' boxes.
"+ Add task" uses it; its row is that much lower, still lit and clicked as a
whole. The task row it opens is a task's height, as before.

**Not taken.** A "+" in the header (saves the whole row, but less obvious);
showing it on hover (the card's height would jump under the pointer).
