# 122: A window's question; text centred on its capitals; the timer window's third run

Status: Accepted (amends 020: a dialog can be over a window; 043: an edit starts all selected and has no box;
094: every line, not only times, is centred on its capitals; 107: the current step is lit only while running;
121: a card's rows are plain list items). Core tested; Windows needs a run.

**Context.** The timer window's third run: "Add task" had two backgrounds and
stayed a "+ Add task" button while typed into; an edit had a box; text sat
low in its rows and fields; "New group" drew the eye like a group; deleting
a group with tasks lost them on one click; the line under the header was
only a line; the current step wasn't the phase's colour in the window, and
on the card it was even with the timer stopped; the header said "Focus ·
round 1 of 4"; a row's highlight met its checkbox's edge; pointing beside
the cards lit a row of the card to the left.

**Decision.**
- **A window's question** (`Dialog::windowId`): `Host::ask` with a window id
  puts a modal over that window (`OpenWindow::dialog`), laid out by
  `core/window_dialog`: everything under the title bar dimmed
  (`windowScrimColor`), a panel (`windowDialogWidth`, `windowDialogPadding`,
  the island's raised colour) with the title, the message, Cancel and the
  confirm button. Only its buttons take a click; Esc cancels, Enter confirms;
  the title bar still drags and closes. Its buttons send the host's
  `kDialogConfirm` / `kDialogCancel`; the registry answers it and only the
  confirm action reaches the provider, as on a card (decision 020).
  Pomodoro asks before deleting a group with tasks ("Delete “Work”? Its 3
  tasks go with it."); an empty group goes at once, with its Undo toast.
- **Text centred on its capitals**: every single line (`drawString`, a
  layout's one line, a field's text, caret and selection) is moved by
  `figureNudge`, as times were (decision 094). Segoe UI's line box has more
  room above its capitals than below its baseline, so text centred by its
  line box sat low in every row, button and field.
- **"+ Add task" is a field that reads as a button** (`TextInput::icon`):
  empty and unfocused, its glyph before its placeholder and no box of its
  own, lit by its row (`litByRow`), the whole row clicking into it. Focused,
  a plain field with its box, no +. An item whose `litByRow` changes is
  rebuilt in place.
- **An edit in place has no box and starts all selected**: typing replaces
  the text, an arrow keeps it, a click in it places the caret (island and
  windows alike).
- **"New group" is quiet**: `GroupFill::dashed`, a dashed outline in
  `trackColor` (`dashedBorderWidth`), no fill; its words a caption, its +
  a secondary button.
- **The header's line is the phase's bar** (`WindowPage::headerProgress`):
  `windowHeaderBarHeight` tall, edge to edge, filling with the session in
  the window's accent. A window's items now get the window's accent.
- **The current step is lit only while the timer runs** (`Steps::lit`), on
  the card and in the window. The header says the phase only.
- **A card's rows are plain list items**: `listItemPadding` on every side of
  their checkbox and buttons, so the highlight never meets them (`Row::dense`
  is gone).
- **Beside the cards no row is lit**: outside every card, `windowRowAt`
  matches only rows that are in no card.
