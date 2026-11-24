# 181: A file dialog has an island of its own, which picks its file

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

Two builds came first, and both were turned down on a real PC:

1. *The island itself moved under the dialog.* Music, alerts and the timer
   don't belong under a dialog.
2. *A hand-drawn pill window, and the island lent to the dialog while its
   card was open* (made invisible, jumped there, shown in the pill's place).
   It flickered whichever way the jump was hidden, came back half way when
   the dialog was dragged, and the pill, drawn apart from the renderer, was
   a black shape a pixel off the dialog. One island can't be in two places.

**Decision.** The dialog gets a second island.

- *Noticing.* `platform/file_dialog_watcher` hooks foreground changes and
  asks `core/file_dialog.h: isFileDialog` about the window in front, by class
  names. An old-style dialog builds its view after it shows, so a bare
  `#32770` is looked at once more, `pickerRecheck` later. While one is in
  front, two more hooks on its thread alone say when it moves or closes.
  The app's own windows in front change nothing (the island's card has the
  keys while the dialog waits); another app's window in front ends it.
- *A second island.* Core was always able to: its state, rules and layout
  are values and pure functions. The dialog's island has its own
  `IslandState`, scene, `PillRenderer`, render layer (with its own
  Compositor), interaction window (`platform/interaction_layer` makes two)
  and `ProviderRegistry`. The app's handlers work on "the island" through
  main.cpp's globals, so the second one stands in their place: `At` swaps
  the two sets for a scope, and each island's window callbacks and its
  registry's dispatch open one. With no dialog in front nothing is swapped,
  so the island runs exactly the code it ran before. One timer serves both.
- *Its one activity.* A second `SearchProvider` (`SearchHooks::forDialog`)
  in that registry, kept in memory only. It's an activity there: a pill
  with a search glyph and "Find a file", whose card is the search card:
  files and folders only, nothing before something is typed. A result is
  given to the dialog (`SearchHooks::giveToDialog`): its path is put in the
  dialog's "File name" field (`isFileNameField` picks it) and its Open /
  Save button is pressed, as if typed. A file is chosen; a folder is gone
  to. Its menu (`pickingMenu`, decision 086): Choose (a folder: Open
  folder), Go to its folder, Copy path; nothing that starts something else,
  and no "Remove from history", as the card shows no history.
- *Where it hangs* (`placeByDialog`). Its layer is the canvas's size with
  the island centred along its top, so it's moved with the dialog as one
  piece, by moving windows, with no morph to lag behind. Resting, it hangs
  from the dialog's bottom edge; with no room there, over the dialog's top
  edge; with none there either, under it, cut off. Its card always opens
  under, and with less than `pickerCardRoom` there moves up over the
  dialog's lower part. The dialog's frame is taken less the one-pixel
  border Windows draws inside it, so no line shows between them.
- *Its own manners.* It can't be lifted. Its card has no top band that
  closes it (`Content::noTopBand`, decision 108): its top is the dialog's
  edge. It fades in and out over `pickerFade`. Search's shortcut, with a
  dialog in front, opens its card instead of the island's, and again closes
  it. Closed with the keys still on it (Esc), the dialog gets them back.

**Consequences.**

- An alert there is its own: "Path copied" shows on the dialog's island.
- A dialog of an app running as administrator doesn't take the path from an
  island that isn't: Windows blocks the message, simulated keys too (UIPI).
  The clipboard is shared, so the path is copied instead, the dialog comes
  in front and its island says "Path copied / Paste it into the File name
  box". Focus is wherever the user left it in the dialog. Double Ctrl isn't
  heard there either: Windows gives a low-level keyboard hook none of an
  elevated window's keys. Listary has the same limits. Getting past them
  means a signed `uiAccess` build; not done here.
- Over the dialog's top edge the island is the shape it is under a screen's
  edge, flat side up: the renderer's shape has no upside-down form yet.
- The dialog's island is drawn at the main island's monitor's scale: on
  another monitor with another scale it's that much larger or smaller.
- In a Save dialog, Choose saves over the picked file; the dialog asks
  first, as it does when the name is typed.
- The text goes with a timeout (`pickerSendTimeout`) and the button press
  is posted, so a hung app or the dialog's own question never holds the
  island (decision 144).
- `At` is a trade: a second island for a few dozen lines, against globals
  that mean "whichever island is being worked on". Anything new that must
  be the main island's alone says so (`g_seat == Seat::Island`, or
  `At island(Seat::Island)`).
- Rules tested in `tests/file_dialog_tests.cpp`,
  `tests/search_picking_tests.cpp` and `tests/scenarios/top_band.scenario`.
  The watcher, the second island's windows and the dialog taking a path
  need a Windows run.
