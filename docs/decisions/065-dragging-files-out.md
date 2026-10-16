# 065: Dragging picked things out of the island

Status: Accepted (the provider's part tested; the drag itself needs a Windows run)

**Context.** Things on the Shelf are there to be taken somewhere: into a
folder, an email, a chat. Explorer does it by dragging. The island can't
let a provider handle the pointer (decision 035), and a provider has no
window to start a drag from.

**Decision.**
- **The host starts it, the provider says what.** A press on a thing in a
  selectable list (decision 064), moved past the system's drag distance
  (`SM_CXDRAG`), drags what's picked. If the press picked it, that's what
  goes; on something already picked, the whole selection goes.
  `Provider::dragOut` returns the files' paths, once, when the drag starts.
  The Shelf gives a copy's file in its store, or a link's original; only
  things that are there (not copying, failed or missing).
- **Windows does the drag** (`platform/drag_source`): the shell's own data
  object for those files (`IShellItemArray`, `BHID_DataObject`) and
  `SHDoDragDrop`, so Explorer and other apps take it as one of Explorer's
  own drags, with its drag image. It offers **copy and link only, never a
  move**: nothing leaves the shelf's store or the user's folder, whatever
  keys are held.
- **The island ignores its own drag** coming back over it: no drop zones,
  nothing lands (decision 061).
- Dragging isn't clicking: the selection stays as it was, and the card
  stays open.

**Consequences.** `provider.h` (`dragOut`), `ProviderRegistry::dragOut`,
`app/selection_input` (the drag start), `platform/drag_source`,
`drop_target.cpp` (ignores our own drag); links `uuid` for the shell's
GUIDs. Tested in `shelf_tests.cpp`.
