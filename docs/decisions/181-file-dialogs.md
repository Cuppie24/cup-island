# 181: A pill under a file dialog picks its file

Status: Accepted

**Context.** Choosing a file in Windows' Open or Save dialog means walking
folders, while the island's search already finds any file in a few letters.
The user asked for the island to come to the dialog: a pill under it, whose
pick goes to the dialog, with a menu that fits.

Windows has no event for "a file dialog opened". Both kinds (today's, and the
old-style one) are a top-level dialog, class `#32770`, with Explorer's file
view inside (`DUIViewWndClassName`, `SHELLDLL_DefView`); message boxes, Print
and Font are `#32770` without it. Apps that draw their own picker can't be
told apart from any other window.

The first build moved the whole island under the dialog. The user turned
that down: music, alerts and the timer don't belong under a dialog. They
also asked for no recent files in the card (what was opened before is
nobody's business while a dialog is on screen, perhaps shared), for the pill
to stay under the dialog even when the screen's edge cuts it off, and for a
single click to open it.

**Decision.**

- *Noticing.* `platform/file_dialog_watcher` hooks foreground changes and
  asks `core/file_dialog.h: isFileDialog` about the window in front, by class
  names. An old-style dialog builds its view after it shows, so a bare
  `#32770` is looked at once more, `pickerRecheck` later. While one is in
  front, two more hooks on its thread alone say when it moves or closes.
  The island's own window in front changes nothing (its card has the keys
  while the dialog waits); another app's window in front ends it.
- *A pill of its own.* `platform/picker_pill`: a small window the idle
  island's size with a search glyph, in the island's shape (flat on the
  dialog, its corners flaring into the dialog's edge, decision 025), hung
  from the dialog's bottom edge, centred (`pillPlace`), on whichever monitor
  the dialog is. With no room under the dialog it stands on the dialog's top
  edge instead, flat side and flares down; with none there either it's
  under it, cut off. (Cut off always, as first asked, hid it behind the
  taskbar with the dialog.) It moves with the
  dialog at once, as one piece, fades in and out over `pickerFade`, and
  never takes the keys. It's an ordinary window, not a topmost one: where
  the dialog goes behind the taskbar, so does the pill, and it's raised
  over the dialog's app each time the dialog comes forward. The island
  stays where it is, as it is.
- *One click lends the island.* A click on the pill hangs the island's
  layers under the dialog (on its monitor, at that monitor's scale) and
  sends `PickerOpened`: core opens the card of the panel with
  `Panel::picksFiles` (Search), centred on the dialog
  (`IslandState::picker`, placed like a Free spot, so decision 026's
  screen-edge rules hold). Search's shortcut does the same while a dialog
  is in front, and pressed again closes the card. The island isn't watched
  making the jump: morphing from the top of the screen to the dialog
  flickered on the first run, so it's made invisible, put there in one step
  and faded in as the pill fades out (and the same on its way home).
  The card needs `pickerCardRoom`: when that isn't under the dialog, it
  opens over the dialog's lower part rather than off the screen. Closed any
  way (a pick, Esc, a click elsewhere, the dialog going, a critical alert),
  `picker` is dropped, the layers go home and the pill is back. A second
  island was the alternative: the renderer, the input window and the app's
  loop are each built for one, and the island is only away while the user
  is typing in the picking card.
- *Picking.* The app tells Search while a dialog is in front
  (`SearchProvider::setPicking`). Its card then lists files and folders
  only, and nothing before something is typed. A result is given to the
  dialog (`SearchHooks::giveToDialog`): its path is put in the dialog's
  "File name" field (`isFileNameField` picks it) and its Open / Save button
  is pressed, as if typed. A file is chosen; a folder is gone to. It ranks
  higher next time like any file opened from search (decision 082).
- *Its menu* (`pickingMenu`, decision 086): Choose (a folder: Open folder),
  Go to its folder, Copy path. Nothing that starts something else: the user
  is in the middle of choosing a file. No "Remove from history": the card
  shows no history.

**Consequences.**

- While the picking card is open, the island isn't at the top of its
  screen; it's back the moment the card closes.
- Search opened from its page-row button while a dialog is in front opens
  in the island's own place, and picks for the dialog too.
- A dialog of an app running as administrator doesn't take the path from an
  island that isn't: Windows blocks the message, simulated keys too (UIPI).
  The clipboard is shared, so the path is copied instead, the dialog comes
  in front and the pill says "Path copied. Paste it into the File name box"
  for `pickerNote`, grown to fit it (`SearchHooks::sayByDialog`; an alert on
  the island only if there's no pill to say it): by the dialog is where the
  user is looking. The user pastes and confirms. Focus is wherever they left it in the
  dialog, which the island can't change. Listary has the same limit. Getting
  past it means a signed `uiAccess` build or a helper started by the
  service; neither is done here.
- In a Save dialog, Choose saves over the picked file; the dialog asks
  first, as it does when the name is typed.
- The text goes with a timeout (`pickerSendTimeout`) and the button press
  is posted, so a hung app or the dialog's own question never holds the
  island (decision 144).
- Rules tested in `tests/scenarios/picker.scenario`,
  `tests/file_dialog_tests.cpp` and `tests/search_picking_tests.cpp`. The
  watcher, the pill, the layers moving and the dialog taking a path need a
  Windows run.
