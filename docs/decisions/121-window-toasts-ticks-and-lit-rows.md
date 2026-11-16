# 121: Windows get the host's toast and their own timer; a card's rows light; no pin

Status: Accepted (amends 104: windows keep time and show toasts; 117/120: a
card's rows; 046: the pin's button is gone). Core tested; Windows needs a run.

**Context.** The timer window's second run: its Undo was a card of its own
among the groups ("these toast notifications"), not the host's toast; its
time only moved when the session was paused or resumed; a card's rows had
no hover background; the group's count wasn't wanted; groups were still too
narrow; and the island's page row had a pin the user didn't want. Also:
every background in a group must nest in the one around it.

**Decision.**
- **A window's toast is the host's** (`Toast::windowId`): `Host::toast` with
  a window id puts it on that open window (`OpenWindow::toast`, one at a
  time, `toastDuration`), not on a card. The window lays it out with the
  island's own `placeToast`, a pill over the page's bottom, and draws it
  with the island's `ToastLayer`, clipped to the page (it slides up from
  under the page's bottom edge). Its button is hit before the page; the
  registry drops it when that button's action is routed, as a card's is.
  Pomodoro's Undo (a task or a group deleted) is one; the Undo card is gone.
- **A window keeps its own time**: after every layout it arms a one-shot
  timer (`ProviderWindow::armTick`) for `windowDeadline`: the next second
  of a playing time label on its page (`sceneDeadline`, now given the
  scene's `now`) or its toast's end. On it, toasts whose time is up go
  (`ProviderRegistry::expireWindowToasts`) and the window is laid out
  again. The island's one timer stays the island's; nothing in a window
  needed a provider update to tick any more.
- **A card's rows are list items, dense** (`Row::dense`, `denseItemPadding`
  3): each lights as a whole under the pointer, from its first item (the +,
  a checkbox) to its last (a delete), 42 px apart. The window lights the row
  `windowRowAt` finds, between its items too, not only the item under the
  pointer. Editable text at a row's edge sits `listItemTextInset` less its
  `editInset`, so its letters line up with plain text's.
- **Radii nest** (decision 006's rule, all the way down): a card's corners
  are `windowCardRadius` (18); its rows' backgrounds that less
  `windowCardPadding` (8); an item in a row its row's less its nearest gap
  to the row's edge (above and below, in a dense row).
- **No count** beside a group's name. **Groups at least 560** wide.
- **No pin in the page row.** `IslandState::stayOpen` and its event stay
  (scenarios), but nothing on screen sets it: it was the pin's alone, and it
  resets whenever the island collapses, so it can't be left on.

**Addendum (decision 122's number, kept here).** "Add task" showed two
backgrounds: its row's, lit, and the field's own box inside it. A field in a
list item is now lit by its row (`Placed::litByRow`): no box of its own, its
click area the whole row (the + too), and its row stays lit while it has the
keys. A Text edited in place keeps its box: it shows what's being changed.
