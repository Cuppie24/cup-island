# 143: A card is dragged by its whole header, with the move cursor

Status: Accepted (amends 129, 139). Core tested; the cursor on Windows needs a run.

**Context.** A group was dragged only by its handle, a short line at its top.
Once its name stopped being edited in place (decision 142), the user asked
for the whole header, name included, to drag it, with a drag cursor over it.
The line stays, as a hint.

**Decision.**
- **Its header picks it up.** `Movable::grip`: a movable card's title row,
  from the card's top to that row's bottom, the card's whole width. A press
  there is held as the handle's is (decision 129): carried, the card moves;
  let go where it was, it's a click, and a click on plain text (the name)
  does nothing. A button on the header (its "…") stays a button. The rows
  under it are still their own: a task drags itself.
- **The move cursor** (`IDC_SIZEALL`) over the header and the handle, and
  while a card is carried: the app asks `movableAt` on each move and tells
  the window (`ProviderWindow::setGrabCursor`), which answers WM_SETCURSOR
  with it over its client area.
