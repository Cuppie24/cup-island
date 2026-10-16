# 066: The island's own context menu; cards without their buttons

Status: Accepted (rules tested; the menu's mouse, keys and look need a Windows run)

**Context.** The Shelf's cards carried hover buttons (Show in folder, Keep
as link, Remove), which crowded small cards and hid behind thumbnails of
the same colour. Explorer puts these in a right-click menu. Windows' own
menu (`TrackPopupMenu`) can't be drawn in the island's style or themes,
and a provider never sees the pointer or keys (decision 035).

**Decision.**
- **The host draws the menu, the provider fills it.** A right-click, or
  Shift+F10 or the menu key, over a card's selectable list (decision 064)
  asks the card's provider once: `Provider::contextMenu(activity, list,
  ids) → MenuItem{glyph, label, actionId, destructive, editsText}`. No
  items: no menu. The menu is `IslandState::menu` (`MenuOpened`,
  `MenuClosed`), laid out by `core/menu_layout` as list rows over the card.
- **What a right-click picks, like Explorer:** on a thing not picked, it's
  picked alone; on a picked thing the selection stays; on the list's empty
  part the selection is cleared. From the keyboard the menu opens at the
  first picked thing.
- **Where:** at the pointer, `menuWidth` wide, flipped left or up and kept
  inside the card. Its items come first in `Scene::elements`, so they win
  the hit and the hover; it draws in its own layer over the card
  (`islandColor` with the tiles' wash).
- **Closing:** choosing an item, Esc (before the selection and the card),
  a press anywhere off its items (which does nothing else), the card
  closing or a dialog opening. While it's open, only its items take the
  keyboard's focus, like a dialog's (decision 033).
- **Choosing:** the action goes to the provider. `editsText` starts
  editing the Text whose `editAction` is the action (Rename, decision 029).
  `menu.selectAll` is the host's: everything in the list.
- **The Shelf's items:** for one thing Open, Show in folder, Rename, Keep
  as link (a failed copy with a source), Remove (asks first); for several,
  Delete N items (asks); always Select all.
- **Cards have no buttons now.** The list view keeps its hover buttons:
  a row has room for them.
- **The list's add row** is clicked across its whole lit background, not
  just up to its button (`slot_layout`, `clickAnywhere` on a `listItem`).

**Consequences.** `island.h` (`MenuItem`, `Menu`, the events),
`card_overlays`, `menu_layout`, `keyboard.cpp`, tokens `menuWidth` and
`menuPadding`; `provider.h` and the registry; the Shelf's provider and
content; `app/selection_input` (`openMenu`), `main.cpp`, the interaction
layer (`WM_CONTEXTMENU`), `blob` (the overlay) and `element_visuals`.
Tested in `tests/scenarios/menu.scenario`, `grid_tests.cpp`,
`shelf_tests.cpp`.
