# 079: The search card, live fields and Double Ctrl

Status: Proposed (builds on 077's launcher and 078's service rules)

**Context.** Search needs a way in, from anywhere, and a card that answers
as you type: apps first, then Settings and Control Panel, then files. The
owner picked Double Ctrl as its default shortcut, as in Listary. Three
things the island didn't have: a text field that reports every keystroke
(fields reported only Enter, decision 016), a shortcut that isn't a key
combination (RegisterHotKey knows only those, decision 032), and a way for a
provider to close its own card once it has started what was picked.

**Decision.**
- **Search is a provider** (`providers/search/`), not part of the host: a
  Search widget on Home and its card, built from the content model like
  any other. Its Windows side (reading the shell's folders, launching) is
  behind hooks, so the rest is tested on Linux. It's granted the service's
  `index` module (decision 078) and gets files only through the host.
- **The card:** a field, then results in one scrolling list, at most
  `searchAppRows` apps, then `searchSettingRows` Settings and Control Panel
  items, then `searchFileRows` files (tokens.json), so a hundred files never
  push the app you meant off the card. Each row: an icon, the name, what it
  is (App, Settings, Control Panel, or a file's folder) and the chevron
  Settings' rows have (decision 030): the whole row is the button. Enter
  starts the top result. Nothing typed, the card lists what was launched
  most, lately. File search not set up or starting is said under the list.
  Answers for an older query are dropped, so fast typing never shows stale
  files. Icons are glyphs for now; apps' own icons need a look on Windows
  first (the content model can carry them, `Image`).
- **Live fields:** `TextInput::changeAction`. Set, every edit (typing,
  erasing, paste, cut) sends that action with the field's whole text; Enter
  still sends `actionId`. The rule is pure (`liveChange`, in
  `core/card_overlays`) and the app applies it after every event.
- **`Host::closeCard(activityId)`:** a provider closes its card if it's the
  one shown and isn't pinned (decision 046); opened from Home, it closes all
  the way. Search calls it once something started, so the island is out of
  the way of the app it opened. A state rule: `home.scenario`.
- **Double-tap shortcuts.** A chord can be `twice`: Ctrl, Shift or Alt on its
  own, pressed twice quickly ("Double Ctrl"). It is one more command in
  decision 032's table, "Search apps and files", rebindable to an ordinary
  chord in Settings like the others (recording a double tap there isn't
  supported yet: once changed, Double Ctrl comes back only by deleting the
  `search=` line from the saved shortcuts, which then falls back to the default).
  - **When it fires** (`core/double_tap`, pure, tested): each tap down and
    up within `doubleTapHoldMs`, the second press within `doubleTapMs` of the
    first's release, nothing else pressed in between, firing as the second
    tap lets go. So Ctrl+C, Ctrl+V and "Ctrl, Ctrl+C" never open search, and
    the island doesn't open while Ctrl is still held (the first letter typed
    would be a Ctrl shortcut).
  - **How it's watched** (`platform/tap_hook`): a low-level keyboard hook
    on a thread of its own (Windows drops a slow hook, and the UI thread may
    be drawing), which only watches: every key still reaches the app in
    front. It posts the command as an ordinary WM_HOTKEY, so nothing after
    it can tell the two kinds apart. Off whenever the other shortcuts are
    off (a fullscreen app, decision 032). WH_KEYBOARD_LL is Windows 2000+
    (decision 015).
  - **Coming to the front:** a real hotkey gives the app that receives it
    leave to take the foreground; a hook doesn't. If SetForegroundWindow is
    refused, a harmless Alt tap first lets it through (`bringToFront`), as
    launchers such as PowerToys do.
- **Opening** (the command): search's card, its field focused and empty
  (see "Second look" below). Pressed again while it's open, the card closes.

**Polish after the first run on a real PC.** The card was laid out like a
media card (art column on the left, a fixed 171 px height), so its rows
started right of centre, the results pushed the field off the top and spilled
over the tab row, and long folder paths squeezed names to "W...". Now:
- `CardLayout::List`, like Settings, and one height whatever the results
  (`Content::sizedLike`: the field and a full window of `searchVisibleRows`
  rows), so the card doesn't jump while typing; the results start at the top
  (`pageAtTop`) and fill what's under the field (a fill list, decision 070).
  A note ("Nothing found") sits under the field, above the list.
- **`Content::fieldFirst`** (core, scenarios in `input.scenario`): a card that
  is its field. Its field has the keys whenever the card is shown, from the
  shortcut or the Home widget alike, with its kept words; one Esc closes it
  (`escapeCloses`) instead of stopping the typing first.
- **`Text::yields`** (core layout): short of room, that text gives up its
  width first. A file's folder yields to its name. (Search's rows no longer
  need it: second look.)
- Each new query scrolls the list back to its top (`Host::reveal`).
- `WindowsApps` (Store apps' insides) is a noise folder (decision 075):
  hundreds of package files no longer crowd out what was meant.
- A layout test lays out the real card with many results and checks each of
  these.

**Consequences.** A keyboard hook in the island: it sees every key press,
so it must stay tiny and do nothing but feed DoubleTap, and it's a reason
some security tools may look at the app; nothing it sees is stored or sent.
Two new core pieces (a live field, a provider closing its card) that any
provider can use.
The hook's thread must be stopped before the app exits (`unregisterHotkeys`):
from 0.8.1 to 0.8.6 it wasn't, and every exit, updates included, ended in
`std::terminate` and a crash dump.

**Not checked yet (Windows):** the hook and Alt-tap foreground, the shell
folders' contents (two ids for "All Control Panel Items" are tried, decision
077), launching Store apps and Control Panel items, and the card's look.

**Keys on the card** (added after the first runs): the field keeps the
keys for typing while **Up and Down** move a highlight through the results,
as in a launcher (`moveAmongRows`, core/keyboard, tested on the real card):
Down from the field is the first row, Up from the first row is back to
typing, Down at the last row stays (the tab row isn't one of the card's rows),
and a list scrolls to keep the highlight in view. **Enter** opens the
highlighted row, or the top result when none is. Typing clears the
highlight (the rows under it change). Left, Right, Home and End stay the
caret's. Any card that is its field gets the same keys.

**Second look** (from the owner's screenshots: too small, a blank strip
under a few results, paths cut where they mattered). This replaces the
fixed height and `Text::yields` above, and the chevrons:
- **Opens empty, grows with results.** Closing the card drops what was
  typed (a fieldFirst card's text isn't kept, unlike decision 050's fields:
  `dropStaleOverlays`), and the provider forgets its query
  (`onCardClosed`). Nothing typed shows the field alone; results add rows
  up to `searchVisibleRows`, then the list scrolls. The card is as tall as
  its rows (`CardLayout::Search`). It jumps as you type; a blank strip
  under two results looked broken, which was worse.
- **Bigger.** `searchCardWidth` wide (canvasSize grew to fit), a
  `searchInputHeight` field in `searchInputFontSize` (`TextInput::large`).
- **Two lines per row:** the name, and under it where it is
  (`Text::detail`, in caption style): App, Settings, or a file's folder with
  the user's own folder as `~`. A path too long **loses its start**
  ("…\projects\cwatis\Debug"), since its end tells two apart; DirectWrite
  only trims ends, so the painter finds the longest end that fits.
- **No chevrons.** The icon is the row's button (`Row::clickAnywhere`): the
  whole row opens it. A file row's **folder button** (`search.reveal.N`)
  shows under the pointer and opens Explorer with the file selected.
- **The row Enter opens is lit** while you type: the first row
  (`defaultRow`). Down goes to the second, Up back onto the first is typing
  again, so every press visibly moves. The pointer over another row lights
  that one, as before.
- **Real icons:** the shell's own, as Explorer shows them
  (`IShellItemImageFactory`, Windows Vista+, on one background thread),
  drawn in the icon button (`Button::picture`, `resultIconSize`); the glyph
  until it comes. Kept by path, up to `searchIconCache`.
- **What matched is brighter and bolder** (`Text::bright`, from
  `matchedParts`: each word where it starts a word in the name, else its
  first match).

**Open.** Ctrl+Shift+Enter to run as administrator (decision 077) needs the
host to tell a provider about modifiers, which it never does (decision 035):
a row button instead, maybe.
