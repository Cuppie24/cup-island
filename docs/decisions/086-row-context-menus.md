# 086: Context menus on a card's rows (first user: search's results)

Status: Accepted; built, not yet run on Windows (builds on decisions 020, 029, 035, 079)

**Context.** A search result does one thing: Enter or a click opens it, and
a file's folder button (shown on hover) reveals it (decision 079). The owner
wants more per result: reveal in Explorer, run as administrator, and others,
reachable from the pointer *and* the keys. Decision 079 left "Ctrl+Shift+Enter
runs as administrator" open because the host never tells a provider about
modifiers (decision 035); a menu answers that without breaking the rule.
Other cards will want menus too (a to-do's Edit / Move / Delete, a widget's
"Hide"), so it's a host component, not search's.

## What the user sees

- **Menus are per row, as many as a card likes.** Every row of a card can
  have a menu of its own, each with its own items (a file's differ from an
  app's on the same card), and lists and plain rows alike. Only one is open
  at a time, as in Windows: opening another closes the first.
- **Every result ends in a `›`** (a quiet chevron, `Glyph::ChevronRight`):
  this row has more. It replaces the hover-only folder button
(`search.reveal.N`, decision 079), which goes: Reveal in Explorer is in the
menu, with its shortcut. Not the
  chevron 079 removed (that read "go into"); this one is a button that opens
  the menu, and its look says the Right key does the same.
- **Right-click on a row:** the menu opens with its top-left corner at the
  pointer, the row lit while it's open.
- **Right arrow on a highlighted row:** the menu opens beside the row, its
  top aligned to the row's top, `menuGap` right of the row's right edge.
  On the search field the caret owns Left/Right (decision 079), so Right
  opens the menu only when the caret is at the end of the text (where Right
  does nothing for typing) or a row was highlighted with Down, so the lit
  row's `›` is always one key away.
- **Click on the `›`:** like Right: beside the row.
- **Out of room:** placed in the screen's band the render layer covers (the
  monitor's width by the canvas's height, decision 026): past the right edge
  it opens to the left (of the pointer, or of the row), past the bottom it
  shifts up. It may overflow the island's shape: it's drawn above the pill,
  not clipped by it.
- **In the menu:** Up/Down move (round from last to first), Home/End,
  Enter/Space choose, Esc or Left close it and give the highlight back to
  its row (one Esc: the menu, not the card), a letter jumps to the next item
  starting with it. A printable key that isn't an item's letter closes the
  menu and types into the field (on a fieldFirst card). Hover lights an item;
  a click chooses it; a click anywhere else, a scroll, or the card closing
  closes the menu. Disabled items are shown dim and skipped.

## The menu's items for search

In this order, a thin line between groups, each shown only where it
applies. The first is the row's own action, bold, so the menu also teaches
what Enter does. Shortcuts are shown at each item's right and work on the
lit row (the one Enter opens, or the one highlighted with Down) without
opening the menu, and inside it too.

| Item | Shortcut | Desktop programs | Store apps | Settings / Control Panel | Files | Folders |
| --- | --- | --- | --- | --- | --- | --- |
| **Open** | Enter | ✓ | ✓ | ✓ | ✓ | ✓ |
| Run as administrator | Ctrl+Shift+Enter | ✓ | — (see below) | Control Panel items that are .exe/.cpl/.msc | .exe, .bat, .cmd, .msc | — |
| — | | | | | | |
| Reveal in Explorer | Ctrl+Shift+E | ✓ its .exe (see below) | — | — | ✓ | ✓ (in its parent) |
| Open in Terminal | Ctrl+Shift+T | the .exe's folder | — | — | its folder | ✓ |
| — | | | | | | |
| Copy | Ctrl+C | — | — | — | ✓ | ✓ |
| Copy path | Ctrl+Shift+C | the .exe's path | — | the `ms-settings:` link | ✓ | ✓ |
| — | | | | | | |
| Remove from history | Shift+Delete | when it ranks from LaunchHistory | same | same | when it's a recent file | same |
| — | | | | | | |
| App settings | | — | ✓ (`ms-settings:appsfeatures-app`, its package) | — | — | — |
| Uninstall | | opens Installed apps (`ms-settings:appsfeatures`) | same | — | — | — |
| — | | | | | | |
| Properties | Alt+Enter | the .exe's | — | — | ✓ | ✓ |

Shortcuts come from Windows' own habits where it has one (Alt+Enter is
Explorer's Properties, Shift+Delete removes a browser's suggestion,
Ctrl+Shift+Enter is the Start menu's "as administrator", Ctrl+Shift+E and
Ctrl+Shift+C are PowerToys Run's, Ctrl+C is Explorer's Copy). None of them
takes a key from the field (Ctrl+A/V/X/Z, Ctrl+Backspace/Delete, Ctrl or
Shift with arrows, Home, End; Ctrl+C copies the file only when no text is
selected, see Copy below), and none uses Ctrl+Alt, which is AltGr on Russian and other layouts
and would type a character. The island's system-wide shortcuts (decision
032) still win: Windows takes them before the island sees the key.

Left out: Open with… (the owner's call), Pin to Start/taskbar (no public
API since Windows 10), Delete (later, maybe, with a dialog).

**Which programs.** "Desktop programs" is every app in the Start menu's
list that isn't packaged: anything installed the usual way (an installer,
a portable app with a Start menu shortcut, Office, Chrome, Visual Studio,
Steam, the island itself). They get Run as administrator, Reveal in
Explorer, Copy path, Open in Terminal and Properties. **Store apps**
(packaged: Calculator, Photos, apps from the Microsoft Store) get none of
those: they can't be started elevated the usual way, and their folder,
`C:\Program Files\WindowsApps`, is closed to the user. One exception to
check on Windows: a few packaged *desktop* apps (Windows Terminal) do offer
"Run as administrator" in the Start menu. If the `runas` verb works on
their shell item, they get it too; until checked, they don't.

**Revealing a program.** Today a program's result has no folder button:
search only knows its shell item (`shell:AppsFolder\<AppUserModelID>`),
not where it lives. The apps reader learns two more things per app:
- **packaged or not:** a packaged app has a package family name
  (`System.AppUserModel.PackageFamilyName`, or a `!` in its
  AppUserModelID);
- **its program file:** the shortcut's target
  (`System.Link.TargetParsingPath` on the item), or, for an app with no
  shortcut, its AppUserModelID when that is a path (a known folder's GUID
  then the rest, `{6D809377-...}\App\app.exe`, resolved with
  `SHGetKnownFolderPath`). Neither: no Reveal or Copy path for it.
Reveal selects the **.exe** in its folder: the program itself, where its
files are. The Start menu's "Open file location" opens the *shortcut* in
the Start Menu folder instead, one more step from the program; the
shortcut is the fallback when the target is gone or isn't a file.
`Launchable` gains `packaged` and `location` (the .exe's path, or empty),
filled by `readApps`; the pure side decides the items from them.

Windows (decision 015, all 1809-safe): `runas` verb on ShellExecuteEx, on
the shell item so a shortcut's arguments stay; Reveal is today's
`SHOpenFolderAndSelectItems`; Properties is `SHObjectProperties` (or the
`properties` verb); Terminal is `wt.exe -d <dir>` when it's installed, else
PowerShell in that folder (Windows 10 has no Terminal by default). Copy
path goes through the app's clipboard helper (`platform/text_input`),
handed in as a hook like Settings' `copyLogs`.

**Copy** puts the file or folder itself on the clipboard, as Explorer's
Copy does: paste it into a folder, an email, a chat. The shell item's own
data object (`IShellItem::BindToHandler(BHID_DataObject)`, Windows Vista+)
through `OleSetClipboard`, marked as a copy (`CFSTR_PREFERREDDROPEFFECT` =
`DROPEFFECT_COPY`), so Explorer pastes a copy and never moves it. Only for
files and folders: copying a program's .exe somewhere isn't what anyone
means, and Settings pages aren't files. Its shortcut is **Ctrl+C**, as in
Explorer, only when the field has no selection: with text selected, Ctrl+C
copies the text as before, so typing is never surprised. Neither Copy nor
Copy path closes the card.

## How it's built

**Content (core/content.h).** A row may carry a menu:
`Row::menu = std::vector<MenuItem>`, `MenuItem{ label, glyph, actionId,
chord, bold, destructive, enabled, group }` (`chord`: an optional
`Chord`, core/shortcuts.h, shown at the right; `group`: items of one
group sit together, a line between groups). Every row has its own. The provider declares items; the host
decides everything else: when it opens, where, how it looks, its keys
(decisions 014, 035). Chosen, the item's action goes to the row's provider
like a button's. A row with a menu gets the `›` from the host (not an item
the provider adds), so every menu looks the same. Search builds its items
in a pure function (`resultMenu(Result, hooks)`, `providers/search/`),
tested on Linux; its action ids are `search.<verb>.<index>`
(`search.admin.3`, `search.reveal.3`, `search.copyPath.3`...).
**Item shortcuts** are the host's too (decision 035: providers never see
keys): on a key, the app first looks for an item with that chord in the
lit row's menu, menu open or not, and sends its action; otherwise the key
goes on as before (the field, the arrows). `menuChord(scene, chord)` in
core, tested.

**State (core, new `menu.h/.cpp`).** `IslandState::menu`:
`{ activityId, rowKey, items, anchor, by (Pointer | Keys), highlighted }`.
Events: `MenuOpened{ rowKey, point? }` (a point: right-click; none: keys),
`MenuHighlighted{ index }`, `MenuChosen`, `MenuClosed`. Rules, each a line in
a new `tests/scenarios/menu.scenario`:
- opens only on a shown card's row that has a menu, never under a dialog;
- one at a time; opening another replaces it;
- Esc closes the menu, not the card; the row keeps the highlight;
- the card closing, a dialog, a scroll, a click outside, typing: closed
  (`dropStaleOverlays` gains it, like the draft and dialog);
- choosing closes it, then sends the action;
- it counts as a touch: an open menu holds the card open (no idle timeout).

**Placement (core, pure, `placeMenu`).** In: the menu's size (from its
items and tokens), the anchor (a point, or the row's rect), the band it may
use (the render layer's: screen width × canvas height, in render-layer
coordinates). Out: its rect. Pointer: top-left at the point, flipped left
or shifted up to fit. Keys: right of the row, top to top; flipped to the
row's left when there's no room. Unit tests for each edge (search's card
near the right edge of the screen, a row near the canvas bottom).

**Scene.** `Scene::menu = std::optional<MenuScene>`: its rect, radius,
each item's frame, label, glyph, state (lit, dim, bold). Laid out by
`layout()` like the toast (decision 071), after the card, so hit-testing
checks it first. `hitTest` gives `MenuItemHit{ index }` or "outside the
menu"; `focusables` while a menu is up are its items only (like a dialog's
buttons).

**Keys (app/keyboard_input.cpp + core/keyboard).** The Right key on a
row with a menu (and Shift+F10 / the Menu key, as Windows does) opens it;
while it's open, every key goes to `menuKey(state, key)` in core: Up, Down,
Home, End, letters, Enter, Space, Esc, Left. No provider sees keys
(decision 035).

**Drawing (platform, new `menu_visuals.cpp`).** Its own visual tree in the
render layer, over the pill and the bubble: a rounded rect in the island's
colours (`menuBackground`, a little lighter than the card, its own
DropShadow as decision 018 does for the pill), rows like a list's lit rows.
It appears with a short scale-and-fade from its anchor corner (`menuOpenMs`
on `menuEase`), and goes without animating. Its geometry is set at once,
never animated from the pill's: the morph's handoffs (decision 008) don't
apply to it, and it never resizes while open.

**Input.** The interaction layer covers only the pill (decision 001). While
a menu is up its input has to reach the menu too. Two ways:
1. *Recommended:* a small input window of its own, like a floating card's
   (`float_window`, decision 081): topmost, no taskbar button, region the
   menu's rounded rect, created when it opens. The pill's window keeps its
   region and its morph replay untouched; clicks between the two reach the
   app under them, which closes the menu (`onClickOutside`).
2. The interaction layer's region grows to the pill plus the menu. One
   window, but its region then changes outside the morph it replays, the
   part decision 001 and 008 say is easy to get wrong.

Keys stay with the island's window (it has the focus); the menu window
never takes it (`WS_EX_NOACTIVATE`), so typing on a fieldFirst card isn't
interrupted.

**Tokens (core/tokens.h + assets/tokens.json).** `menuMinWidth`,
`menuMaxWidth`, `menuItemHeight`, `menuPadding`, `menuRadius`, `menuGap`,
`menuFontSize`, `menuIconSize`, `menuBackground`, `menuDisabledText`,
`menuOpenMs`, `menuEase`. No sizes in code.

## As built

The plan above, with these differences (the Windows side compiles in CI but
hasn't been run yet):
- **Input: the interaction window grows, no window of its own.** The island's
  input window is already a bounding rectangle (the pill, plus the bubble in
  Split), not a shaped region, so `interactiveBounds` simply covers the open
  menu too, as it does the bubble. A press anywhere while the menu is open is
  the menu's: on an item, it's chosen on release; elsewhere it only closes
  the menu, as Windows' menus do. To check on Windows: the window following
  that change (decision 001's morph replay) when nothing else moves.
- **Drawn** by `platform/menu_layer`, in the render layer's root over the pill
  and the bubble: the raised colour a toast uses (`islandHighlightColor`), a
  faint `trackColor` edge, its lit item in `buttonHoverColor`, lines in
  `trackColor`. No shadow and no colour tokens of its own: every theme's
  colours fit as they are. It grows from where it opened
  (`contentArriveScale`) and fades in over `menuOpenMs`, and goes at once.
  Laid out in core (`placeMenu`), like the toast.
- **Tokens:** `menuMinWidth`, `menuMaxWidth`, `menuItemHeight`,
  `menuPadding`, `menuItemInset`, `menuRadius`, `menuGap`, `menuLineGap`,
  `menuOpenMs`. Its text is Body (shortcuts Caption), the bold item bright.
- **The island's card only.** A floating card's overlays can hold a menu,
  but it isn't drawn there yet, so it doesn't open there.
- **Control Panel items** get Open only: their shell items don't say which
  program they are.
- **Code:** `core/menu` (state, keys, placement, hit-testing),
  `providers/search/result_menu` (search's items), `app/menu_input` (the
  pointer, the keys, shortcuts; the WM_CHAR after a key it took isn't typed),
  `platform/menu_layer`, `windows_apps` (runas, Terminal, Properties, copying
  a file, an app's package and program file). Tests: `menu.scenario`,
  `menu_tests.cpp`, `search_tests.cpp`.

To check on Windows: placement at the screen's edges at 100% and 150%; the
field keeping the keys while a menu is open; Reveal on Chrome, Office, a
portable app and an app with no shortcut; runas on Windows Terminal (a
packaged desktop app: not offered yet); `ms-settings:appsfeatures-app` with a
package family name; Alt+Enter not beeping.

## Settled with the owner

- Right on the field opens the menu when the caret is at the end of the
  text, and on any row highlighted with Down.
- Items show shortcuts, and the shortcuts work without the menu.
- The menu replaces the file rows' hover folder button.
- No Open with….

## Open

- Submenus: not in the first version.
- Rebinding item shortcuts in Settings (decision 032's page): later.

**Consequences.** A second overlay kind besides dialogs and toasts, with
its own window while open. Any card can give its rows a menu, which is
the way to "more actions" from now on, instead of hover buttons.
