# 064: Picking things like files in Explorer; the Shelf's add card

Status: Accepted (rules tested; the mouse, the keys, the picker and the drawing need a Windows run)

**Context.** The Shelf holds files, and people expect files to behave the
way Explorer's do: a click selects, Ctrl and Shift add to the selection, a
box drawn with the mouse picks what it touches, Ctrl+A, Delete, Enter, F2.
A provider can't do that itself: keys and the pointer never reach it
(decision 035). So the host keeps the selection, and tells the provider.

**Decision.**
- **A list or grid can be selectable** (`Row::selectable`). Its rows (or
  cells, `Cell::fixed` excepted) are picked by their ids (`InList::item`).
  The selection (`IslandState::selection`: the card, the list, the ids in
  the list's order, an anchor) lives in core, goes with its card, and
  drops ids that disappear. Picked things are lit like the active tab
  (`Scene::selected`, `tabSelectedColor`, so every theme has it).
- **Mouse** (`app/selection_input`):
  - a click picks one; Ctrl+click adds or takes one away; Shift+click picks
    the range from the anchor; Ctrl+Shift adds the range;
  - a press on something already picked waits for the release, so it
    doesn't undo a multiple selection;
  - a press on the list's empty part starts a box (`selectionBoxColor`)
    that picks what it touches; with Ctrl it adds to what was picked;
  - buttons on a thing are their own click, never a pick;
  - a double-click opens it (decision 063).
- **Rename:** a click on a thing's name picks it alone and renames it at
  once (first tried as Explorer's slow second click; too slow to find).
  F2 renames the one thing picked.
- **Keys:** Ctrl+A picks everything; Delete sends the card's
  `Content::deleteAction`; Enter opens the one thing picked; F2 renames
  it; the arrows move the keyboard's focus and pick what it lands on
  (Shift extends, Ctrl only moves); Esc lets go of the selection first,
  then steps back as before (decision 033). A right-click, Shift+F10 or
  the menu key opens the island's menu (decision 066), and Esc closes it
  before anything else.
- **The provider is told** (`Provider::onSelectionChanged`) and acts on it:
  the Shelf's foot reads **Delete N items** while something is picked,
  else Clear; Delete asks first.
- **Removing one asks too**, and says whether the original stays (a link)
  or the copy is deleted.
- **The add card** (the grid's last cell, `fixed`; the list's last row)
  opens Windows' file picker (`IFileOpenDialog`, several files). Picked
  files are kept as copies by the Copy zone's rules (decision 061); what
  can't be copied (too large, a folder) is linked, and a toast says how
  many. The card stays open under the picker (`g_picking` keeps a click
  outside from closing it).

**After a first look.** The selection and the box show at once, no fade:
a fade lagged behind the pointer. The add card is one button, lit as a
whole (`Cell::clickAnywhere`); the list's add row too. A name's hover is
its whole stretch and a field's height (`Text::editHint`), the text drawn
where it was.

**Also in this round.** A backed button's disc is the same shape as its
hover (both circles), and the hover lightens it: the disc is a shape under
the hover, not paint in the glyph's surface. List rows give the name the
room: the kind and size are a fixed column (`Text::column`,
`listColumnWidth`). Cards are smaller again (`gridCellMinWidth` 112,
`gridCellHeight` 132, `thumbnailHeight` 72).

**Consequences.** `content.h` (`Row::selectable`, `Cell::fixed`,
`Content::deleteAction`, `Text::column`), `island.h` (`Selection`,
`ItemSelected`, `SelectionSet`, `SelectionCleared`), `core/selection`,
`provider.h` (`onSelectionChanged`), `app/selection_input`,
`platform/blob` (selection and box marks), `element_parts` (the disc).
Tested in `tests/scenarios/selection.scenario`, `grid_tests.cpp` and
`shelf_tests.cpp`. Dragging picked things out: decision 065. Not yet: scrolling
while a box is drawn past the list's edge.
