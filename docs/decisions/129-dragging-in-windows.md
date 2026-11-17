# 129: Dragging in windows: tasks and groups

Status: Accepted (amends 106: lists in windows didn't drag). Core tested; the feel on Windows needs a run.
Amended by 134: one handle line, gliding, a drop outline.

**Context.** The user asked to drag tasks (within a group, and between
groups) and groups (by a short drag line shown at a group's top on hover).
Windows had no dragging: a page scrolls as a whole and `Row::list`'s own
dragging (decision 029) is the island's.

**Decision.**
- **A provider says what moves**: a row in a card with a `Row::moveAction`;
  a card with a `WindowSection::moveAction`; a card that takes rows dragged
  from another with `WindowSection::takesRows`.
- **Laid out** (`placePage`): each movable row's span and each movable card's
  frame (`WindowScene::movables`), the cards rows land in (`dropZones`), and
  a movable card's handle: `Glyph::Grip` (GripperBarHorizontal), a
  `smallButtonSize` × `windowDragHandleHeight` button at its top's middle,
  `revealWithSlot`: shown while the pointer is anywhere on its card
  (`windowCardAt`; `ElementVisuals::setHoveredRow(row, slot)`).
- **Pure drag logic** (`core/window_drag`): what a press picks up
  (`movableAt`: a handle's card, else a row); where it would land
  (`dropTarget`, worked out on the page laid out without the drag, so what
  makes room doesn't move the target: a row by its middle among a card's rows,
  over no card back where it was; a card in reading order among the cards);
  the content with it moved there (`withMove`: laid out, the others make
  room); the carried thing following the pointer (`follow`); and, let go,
  `Action{moveAction, value = index, text = the card it lands in}`, none if
  it lands where it was (`dropAction`).
- **The app** (`app/provider_windows`): a press on something that moves is
  held. Carried `dragStartDistance` it's a drag: a field with the keys lets go
  first (a row being written is added, decision 127), the window is laid out
  as above on every move, nothing under it lights. Let go, the action goes to
  the provider; let go outside the window, or Esc, and it goes back. Never
  carried, the held press is pressed and released as before, so a click on a
  task's box, its text or its delete works as it did. A press in the field
  with the keys places its caret and picks nothing up.
- **Pomodoro**: a task row's `task.move.<group>.<task>`, a group's
  `group.move.<group>`; groups take tasks. `TaskBoard::moveTask` takes it
  out of its group (`TodoList::take`) and places it in the other
  (`TodoList::place`: a new id of that list's, an open one's rank between its
  neighbours', a solved one stays under the open ones); `moveGroup` reorders.
  Both are saved.

**Not taken.** Scrolling by itself near the pane's edges while carrying:
the wheel scrolls during a drag. Lifting the carried row above the others:
they make room, so it rarely covers one.
