# 180: By a file dialog, the island hangs under it and picks its file

Status: Accepted

**Context.** Choosing a file in Windows' Open or Save dialog means walking
folders, while the island's search already finds any file in a few letters.
The user asked for the island to come to the dialog: a pill under it, opened
with a double-click, whose pick goes to the dialog, with a menu that fits.

Windows has no event for "a file dialog opened". Both kinds (today's, and the
old-style one) are a top-level dialog, class `#32770`, with Explorer's file
view inside (`DUIViewWndClassName`, `SHELLDLL_DefView`); message boxes, Print
and Font are `#32770` without it. Apps that draw their own picker can't be
told apart from any other window.

**Decision.**

- *Noticing.* `platform/file_dialog_watcher` hooks foreground changes and
  asks `core/file_dialog.h: isFileDialog` about the window in front, by class
  names. An old-style dialog builds its view after it shows, so a bare
  `#32770` is looked at once more, `pickerRecheck` later. While one is in
  front, two more hooks on its thread alone say when it moves or closes.
  The island's own window in front changes nothing (its card has the keys
  while the dialog waits); another app's window in front ends it.
- *One island, moved.* The island itself goes there, not a second pill: its
  layers hang from the dialog's bottom edge instead of their monitor's top
  (`hangUnder`), and core centres the pill on the dialog
  (`IslandState::picker`, placed like a Free spot, so the screen-edge rules
  of decision 026 hold). Its saved spot isn't touched, and it can't be lifted
  there. With less than `pickerPillRoom` under the dialog, or the dialog on
  another monitor, it stays in its own place, still picking. A card needs
  `pickerCardRoom`: when that isn't there, the layers move up over the
  dialog's lower part while the card is open, rather than off the screen.
- *Double-click.* By a file dialog a click on the resting pill (or the
  bubble) does nothing, and a double-click opens the card of the panel with
  `Panel::picksFiles`: Search. Alerts and an open card take a click as ever;
  the shortcuts work as ever. That card closes when the dialog goes.
- *Picking.* The app tells Search (`SearchProvider::setPicking`). Its card
  then lists files and folders only, the recently opened ones before
  anything is typed. A result is given to the dialog
  (`SearchHooks::giveToDialog`): its path is put in the dialog's "File name"
  field (`isFileNameField` picks it) and its Open / Save button is pressed,
  as if typed. A file is chosen; a folder is gone to. It counts as a recent
  file from then on (decision 082).
- *Its menu* (`pickingMenu`, decision 086): Choose (a folder: Open folder),
  Go to its folder, Copy path, Remove from history. Nothing that starts
  something else: the user is in the middle of choosing a file.

**Consequences.**

- Everything the island shows (music, alerts, the timer) is under the dialog
  while one is in front, and back when it goes.
- A dialog of an app running as administrator doesn't take the path from an
  island that isn't: Windows blocks the message. The card stays open then.
- In a Save dialog, Choose saves over the picked file; the dialog asks
  first, as it does when the name is typed.
- The text goes with a timeout (`pickerSendTimeout`) and the button press
  is posted, so a hung app or the dialog's own question never holds the
  island (decision 144).
- Rules tested in `tests/scenarios/picker.scenario`,
  `tests/file_dialog_tests.cpp` and `tests/search_picking_tests.cpp`. The
  watcher, the layers moving and the dialog taking a path need a Windows run.
