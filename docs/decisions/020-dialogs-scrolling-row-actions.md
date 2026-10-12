# 020: Dialogs, scrolling lists, and buttons shown on hover

Status: Accepted; scrolling superseded by 029 (the host scrolls lists)

**Context.** Pomodoro's to-do list needed four things the island didn't
have: scrolling past the first few tasks, a way to ask "are you sure?"
before clearing, a delete button per task, and keeping that button out of
the way until it's wanted. Other providers will want the same (a file
shelf, notifications), so each is a host feature, not a Pomodoro trick.

**Decision.**
- **Dialog** (`Host::ask(Dialog)`): a provider asks a question over its
  own card: title, message, a confirm label and action, and `destructive`
  (the confirm label in `dangerColor`). The host shows it *in place of the
  card's content*, at the card's size, so nothing morphs when it opens or
  closes; Cancel and the confirm button are the host's (reserved action
  names, `core/dialog.h`). Confirm sends the provider `confirmAction`;
  Cancel and Esc close it with the card still open. It only opens over the
  card that's shown, it stops any typing, and it goes with its card
  (`card_overlays`, like the text draft). It's drawn inside the island,
  not as a separate OS window: a popup window would steal focus, sit apart
  from the island, and need its own theming; inside, it's one more
  presentation of the card, with the island's look.
- **Scrolling** (`Row::scrollAction`): the mouse wheel over a row that has
  one sends that action with the steps (positive: down). The provider moves
  its window and republishes, so the host stays unaware of lists and the
  provider's logic stays testable. Touchpads' fractions of a notch add up.
- **Buttons shown on hover** (`Button::revealOnHover`): hidden until the
  pointer is anywhere on its row, then faded in. A to-do's delete button.
- **`Row::clickAnywhere`** now spans only the row's `items`, so a button at
  the row's edge (the delete) gets its own clicks instead of ticking the task.
- **List items** (`Row::listItem`): `listItemPadding` above and below, so
  items stand apart, content inset by `listItemInset`, and the whole row lit
  under the pointer (`hoverColor`, the same mark as a hovered tab or tile).
  Layout gives each item its row's frame (`Placed::listRow`); the app aims
  the mark. A row's mark survives a re-layout of the same card, or it would
  blink with every tick of a countdown beside it. The row's tick button
  (the checkbox) has no hover shape of its own (`Placed::litByRow`): the
  lit row already says it's clickable. The delete button keeps its own.

**The to-do list uses them:** a window of `todoVisibleItems` tasks that
scrolls with the wheel, a caption for what's out of it ("2 above · 3
below"), adding scrolls to the new task; a delete button per task on hover
that deletes at once; Clear solved and Clear all ask first ("Clear all
tasks? All 5 tasks will be removed." · Cancel · Clear all). Each task is a
list item. Pomodoro's stop button asks too ("Stop the session?"): ending
loses the session's progress, and it sits next to pause.

**Consequences.** Rules in core (`card_overlays`, `dialog`, `slot_layout`),
tested by `tests/scenarios/dialog.scenario` and `tests/todo_tests.cpp`.
The wheel, the hover reveal and the dialog's look need a Windows run.

**Open.** No scroll bar, only the caption. Keyboard focus doesn't move
between the dialog's buttons (Enter isn't confirm); Esc cancels.
