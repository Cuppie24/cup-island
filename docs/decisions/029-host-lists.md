# 029: The host scrolls, reorders and edits lists

Status: Accepted (supersedes 020's scrolling)

**Context.** Decision 020 had Pomodoro scroll its own to-do list: the wheel
sent an action, the provider moved a window of whole rows and republished.
Every notch jumped a row, and a smooth glide was impossible: only the
compositor can animate between frames, and it never saw a scroll. Reordering
by drag and editing a task in place had the same problem: they are pointer
and keyboard work that belongs to the host, with the result going to the
provider. Other providers will want lists too.

**Decision.**
- **Lists** (`Row::list`): rows with the same list id, back to back, are a
  list. The provider publishes all of it; the host shows a window
  `listVisibleRows` rows tall (4.5: the half row says there's more), clips
  the rest, and makes only what's inside clickable.
- **Scrolling** is core state (`IslandState::scrolls`, by card and list).
  The wheel sends `ListScrolled` with the offset shown and the maximum from
  the scene, so core clamps without knowing heights. Touchpads' fractions of
  a notch go through as they come. On the compositor each list is a clipped
  container with a scroller inside; the scroller's offset eases
  (`scrollDuration`), and a new step mid-glide carries on from where it is.
  Elements sit at their unscrolled place in the scroller, so scrolling
  doesn't touch them.
- **Keeping a row in view** (`Host::reveal`): Pomodoro asks for a new
  to-do. Layout scrolls the least that shows it, until the wheel takes over.
- **Reordering** (`Row::moveAction`, `sortGroup`): a press on a row dragged
  `dragStartDistance` picks it up (and is no longer a click). The row
  follows the pointer at once; the others ease aside to show where it would
  land, only among rows of its group (open to-dos above solved ones). Let
  go, the drag ends first, then the provider gets `moveAction` with the new
  index, so the row's glide back carries on to its new place.
- **Row ids** (`Row::id`): element keys use them instead of the row's
  index, so a task's visuals follow it when the list reorders (a drop, a
  tick moving it between groups) and glide instead of changing text in place.
- **Editing in place** (`Text::editAction`, `Button::editsText`): the edit
  button turns its row's text into a field holding that text (a draft that
  starts with it). Enter sends `editAction` with the new text and ends the
  edit; Esc leaves it as it was. While editing, clicks on the row go to the
  field, not the checkbox.

**The to-do list uses all of it:** open ones first, solved below, each in
its own order; edit and delete on hover; drag to reorder within a group;
adding one scrolls it into view.

**Consequences.** Rules and planning in `core/lists`, tested by
`tests/scenarios/lists.scenario` and `tests/todo_tests.cpp`. The
compositor side is `platform/list_viewports`. Pomodoro no longer keeps a
scroll position or a "n above · n below" caption.

**Open.** No scroll bar, and no auto-scroll while dragging near the
window's edge: drag within what's shown, or scroll first. Keyboard
reordering isn't there.
