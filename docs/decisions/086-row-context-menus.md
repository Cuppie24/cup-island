# 086: Context menus on a card's rows (first user: search's results)

Status: Proposed (a plan; builds on decisions 020, 029, 035, 079)

**Context.** A search result does one thing: Enter or a click opens it, and
a file's folder button (shown on hover) reveals it (decision 079). The owner
wants more per result: reveal in Explorer, run as administrator, and others,
reachable from the pointer *and* the keys. Decision 079 left "Ctrl+Shift+Enter
runs as administrator" open because the host never tells a provider about
modifiers (decision 035); a menu answers that without breaking the rule.
Other cards will want menus too (a to-do's Edit / Move / Delete, a widget's
"Hide"), so it's a host component, not search's.

## What the user sees

- **Every result ends in a `›`** (a quiet chevron, `Glyph::ChevronRight`):
  this row has more. It replaces the hover-only folder button. Not the
  chevron 079 removed (that read "go into"); this one is a button that opens
  the menu, and its look says the Right key does the same.
- **Right-click on a row:** the menu opens with its top-left corner at the
  pointer, the row lit while it's open.
- **Right arrow on a highlighted row:** the menu opens beside the row, its
  top aligned to the row's top, `menuGap` right of the row's right edge.
  On the search field the caret owns Left/Right (decision 079), so Right
  opens the menu only when the caret is at the end of the text (where Right
  does nothing for typing) or a row was highlighted with Down. Recommended:
  both, so the lit default row's `›` is always one key away.
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

The first item is the row's own action, bold, so the menu also teaches
what Enter does. Shown only where it applies:

| Item | Apps (desktop) | Store apps | Settings / Control Panel | Files | Folders |
| --- | --- | --- | --- | --- | --- |
| Open | ✓ | ✓ | ✓ | ✓ | ✓ |
| Run as administrator | ✓ | — | Control Panel items that are .exe/.cpl | .exe, .bat, .cmd, .msc, .ps1 | — |
| Reveal in Explorer | the shortcut's target | — | — | ✓ | ✓ (its parent) |
| Open with… | — | — | — | ✓ | — |
| Open in Terminal | — | — | — | its folder | ✓ |
| Copy path | target path | — | the `ms-settings:` link | ✓ | ✓ |
| Copy name | ✓ | ✓ | ✓ | ✓ | ✓ |
| Properties | the target's | — | — | ✓ | ✓ |
| App settings | — | ✓ (`ms-settings:appsfeatures-app`, with its package name) | — | — | — |
| Uninstall | opens Installed apps (`ms-settings:appsfeatures`) | same | — | — | — |
| Remove from history | when it ranks from LaunchHistory | same | same | when it's a recent file | same |

Suggested beyond the two asked for, in order of value: **Copy path**,
**Open with…**, **Properties**, **Remove from history** (pure, no Windows
needed), **Open in Terminal**, then **App settings / Uninstall**. Left out:
Pin to Start/taskbar (no public API since Windows 10), Delete (a dialog
and a recycle-bin call for something one keystroke away; later, maybe).

Windows (decision 015, all 1809-safe): `runas` verb on ShellExecuteEx
(Store apps can't be elevated: not offered); Reveal is today's
`SHOpenFolderAndSelectItems`; Open with… is `SHOpenWithDialog` (Vista+);
Properties is `SHObjectProperties` (or the `properties` verb); Terminal is
`wt.exe -d <dir>` when it's installed, else PowerShell in that folder
(Windows 10 has no Terminal by default); a desktop app's target is the
shortcut's `System.Link.TargetParsingPath`. Copy goes through the app's
clipboard helper (`platform/text_input`), handed in as a hook like
Settings' `copyLogs`.

## How it's built

**Content (core/content.h).** A row may carry a menu:
`Row::menu = std::vector<MenuItem>`, `MenuItem{ label, glyph, actionId,
bold, destructive, enabled }`. The provider declares items; the host
decides everything else: when it opens, where, how it looks, its keys
(decisions 014, 035). Chosen, the item's action goes to the row's provider
like a button's. A row with a menu gets the `›` from the host (not an item
the provider adds), so every menu looks the same. Search builds its items
in a pure function (`resultMenu(Result, hooks)`, `providers/search/`),
tested on Linux; its action ids are `search.<verb>.<index>`
(`search.admin.3`, `search.reveal.3`, `search.copyPath.3`...).

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

## Steps (each its own PR, tests first)

1. **Core:** `Row::menu`, `MenuItem`, `menu.h` state and events,
   `placeMenu`, `Scene::menu`, hit-testing and keys; `menu.scenario`;
   unit tests for placement and keys. No visible change yet.
2. **Search's items:** `resultMenu`, the action ids, new SearchHooks
   (`runAsAdmin`, `openWith`, `properties`, `openTerminal`, `copyText`,
   `targetOf`); `search_tests.cpp` checks each kind's items and that each
   action calls its hook and closes the card when something started.
   Remove from history is pure and done here.
3. **Platform:** `menu_visuals`, the menu's input window, right-click
   (WM_RBUTTONUP on the interaction layer), the `›` drawn by the host. Needs a
   Windows run: placement at screen edges, at 100% and 150% scaling, shadow,
   that the field keeps the keys while the menu is open.
4. **Hooks on Windows** (`windows_apps.cpp`): runas, SHOpenWithDialog,
   SHObjectProperties, Terminal with its fallback, the link target.
5. Decision 079's "Open" note closes; ARCHITECTURE.md's Providers and
   Status get a line.

## Open questions

- Right on the field with the caret at its end opens the menu (above). If
  that feels surprising, only Down-highlighted rows open it.
- Should Ctrl+Enter / Ctrl+Shift+Enter become menu shortcuts shown at the
  items' right ("Run as administrator   Ctrl+Shift+Enter")? It needs the
  host to map a chord to a menu item (still no keys for providers): an
  optional `MenuItem::chord`.
- Submenus (Open with → each app): not in the first version.

**Consequences.** A second overlay kind besides dialogs and toasts, with
its own window while open. Any card can give its rows a menu, which is
the way to "more actions" from now on, instead of hover buttons.
