# 063: The Shelf's card: a large card, a grid of cards, double-click opens

Status: Accepted (amends 047's one card width and 035's hints; the Windows side needs a run)

**Context.** The Shelf's first card (decision 061) was a 440-wide list with
a link or copy glyph per row and an "open" button. That's cramped for files:
you want to see what they are (a thumbnail), name them, open them the way
Explorer does, and find the one you just dropped. Several things that
needs weren't in the content model yet: a card bigger than every other
card (decision 047), a grid, a double-click, a provider pointing the
keyboard's focus somewhere (decision 035), and an image smaller than its slot.

**Decision.**
- **A Large card preset** (`CardLayout::Large`, `largeCardSize`, first 760×540, now 700×420):
  stacked like a Centred card, but at its own size, and a list in it fills
  what its other rows leave. `canvasSize` grows to 820 wide so it and its
  shadow fit. Every other card keeps decision 047's width; Large is for
  cards whose content is a collection. Receptive grows too (first 760×320, now
  620×200), so the drop zones are big and a drop morphs only a little into
  the card. Both were made smaller after a first look: they covered too
  much of the screen.
- **Grids** (`Row::cells`, `core/grid_layout`): a row of cells, wrapped
  into as many columns as fit at `gridCellMinWidth`, each `gridCellHeight`
  tall. The host turns them into lines before planning lists, so a grid in
  a list scrolls like one, and `Host::reveal` names a cell. A cell's rows
  are stacked in it; its `actions` sit in its top-right corner. The whole
  cell is one hover group (`Placed::group`, `hoverGroup`): lit together,
  and its `revealOnHover` buttons show together. Home's tile grid stays its
  own thing: its widgets are panels, not a provider's content.
- **Image roles** (`Image::role`): Art (the slot's square, as before),
  Icon (an icon's size, in a list row), Thumbnail (`thumbnailHeight` across
  its row: a cell's picture). `Image::contain` fits a picture inside its
  frame instead of cropping it: a file type's icon must not be cut.
- **Double-click** (`Row::openAction`, `Cell::openAction`, carried on every
  element as `Placed::openAction`): a second click on the same row or cell
  within the system's double-click time and distance sends it, instead of a
  second click. Never through a button on it (Remove stays Remove).
- **Focus as a hint** (`Host::reveal(…, focus = true)`): the keyboard's
  focus goes to that row's or cell's first stop, once (`focusRequest`, then
  `FocusTaken`). The host still owns focus (decision 035): a provider says
  where something new is, never moves focus around; a real pointer move
  takes the lead back, as always.

**The Shelf with it.**
- Cards by default, or a list; the toggle (top right) is remembered (`view`).
- A card: Windows' picture of the file (its thumbnail, else its type's icon,
  `IShellItemImageFactory`), read once its card has been seen and kept in
  memory only; a badge saying link or copy (decision 061) before its name;
  what it is or how it's doing under it.
- Its buttons only on hover, in both views: **Show in folder** (Explorer
  with it selected, `SHOpenFolderAndSelectItems`; only while there's a
  file there to show), **Keep as link** (a failed copy), **Remove**.
- **The name renames it** (decision 043's edit in place): a link only on the
  shelf (`Entry::label`), never the user's file behind their back; a copy's
  file in the store (`MoveFileEx` in its own folder). Never to a name
  ending in `.partial`, which the next start deletes.
- **A double-click opens it**, as Explorer would; a folder opens in Explorer.
- A drop goes first, scrolled to, with the keyboard's focus.
- The widget's icon is a box (Package).

**After a first look on Windows.**
- **A click outside closes it after a drop too.** OLE never activates the
  window under a drop, so no WM_ACTIVATE comes when the user clicks
  elsewhere. After a drop the island asks to come to the front; and while
  it's open but not the foreground window, a low-level mouse hook
  (`platform/outside_clicks`) watches for a press outside it. Only then:
  the hook is off whenever the island is active or closed.
- **Buttons over a picture have a backdrop** (`Button::backed`): a disc of
  the island's colour (`buttonBackdropOpacity`), so they read over a
  thumbnail of the same colour.
- **The link/copy badge is small** (`Icon::compact`, `smallIconSize`), by the
  "Link" or "Copy" under the name, with the item's size.
- **A press doesn't squash the island on this card** (`Content::steadyPress`):
  the provider's call, for a page of things to click.
- **Smaller cards** (`gridCellMinWidth` 128, `gridCellHeight` 150,
  `thumbnailHeight` 88).
- **Clear sits at the card's foot**, with a bin (`Button::withGlyph`): a
  list filling a Large card takes all the room left, however few its rows,
  so what's under it never moves.
- **The total size** of what's on the shelf (sizes known; folders aren't),
  beside the count.
- **A name lights under the pointer** (`Text::editHint`), like a field, so
  it reads as something a click renames.

**Consequences.** `content.h` (`Cell`, `Row::cells`, `Row::openAction`,
`ImageRole`, `CardLayout::Large`), `layout.h` (`Placed::openAction`,
`group`, `InList::cell`), `core/grid_layout`, `lists.cpp` (a filling list),
`item_metrics.cpp` (image roles); `main.cpp` (double-click),
`keyboard_input.cpp` (the focus hint), `element_visuals.cpp` (hover
groups), `element_painting.cpp` (contained images, glyphs),
`platform/shell_images`, `providers/shelf/`. Tested in
`tests/grid_tests.cpp`, `tests/shelf_tests.cpp`, `lists.scenario`. A
double-click on a name starts renaming it (its first click does): open from
the picture or the empty part of a card or row.
