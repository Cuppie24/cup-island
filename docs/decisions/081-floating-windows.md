# 081: A card can open as its own window

Status: Accepted (core done; the Windows side is next)

**Context.** A card grows out of the island and covers the top of the
screen while it's open. Some cards are better kept beside other work, like
a regular window: moved, resized, in front of or behind other apps, while
the island goes on showing its pill. Several providers may want this at
once.

**Decision.**
- **The provider asks, the host decides how** (decision 014):
  `Content::window` (a `WindowSpec`): a size class (`Small`, `Medium`,
  `Large`, sizes in tokens.json), `resizable`, where it first opens
  (`NearIsland`, under the pill, or `Centre`), and `escCloses`. Everything
  else is the host's: sizes, the title bar, the motion.
- **Wherever a card would open on the island** (a click on the pill, the
  bubble, an alert, a tab, a Home widget, the activity shortcut), a card
  with a `window` opens in its window instead, or brings it to the front if
  it's open. The island goes back to its pill, unless it's pinned open
  (decision 046). A second click does nothing more: no hiding it again.
  Windows only ever open from these user actions.
- **One window per card, several at once**, from several providers:
  `IslandState::windows`. `windowsPerProvider` caps one provider; past it,
  the island shows the card, so the click isn't lost.
- **Focus decides where keys go.** Windows gives the keys to one window at a
  time; the platform says which (`WindowFocused`), and keys, typing, Esc,
  and answering a dialog or toast go to that card (`focusedCard`). So
  there's still one `Draft` and one list drag at a time. A window losing
  the keys keeps its field's text, as any field does (decision 050).
- **Each window has its own dialog and toast**: a question can wait over
  one window while the user works in another. `dialogOver` and `toastOn`
  find them by card, wherever it is.
- **Closing:** the ✕ (host-owned action `kCloseWindow`), Alt+F4 (unlike the
  island, decision 049), Esc once the card has no step back
  (`Content::backAction`, decision 033) unless `escCloses` is false, the
  provider closing its card (`Host::closeCard`), or the card going away.
  An activity ending with a panel behind it leaves the dormant card, as on
  the island (decision 009). Closing destroys the window's visuals; the
  provider gets `onCardClosed`, as for a card on the island, and keeps its
  own state.
- **Windows stay open** when a fullscreen app hides the island (they're
  ordinary windows) and under a critical alert.
- **Providers barely change:** card callbacks already come per activity, so
  `onCardShown` / `onCardClosed` fire for several cards at once
  (`shownCards`, `cardChanges`).
- **Its look** (`windowScene`): our own title bar, `windowTitleBarHeight`
  tall, the card's label at the left and a ✕ at the right; the rest of the
  bar drags the window (`Scene::titleBar`). The card under it is laid out
  as Expanded lays it out (`cardSlots`), at the window's size, so it looks
  the same as on the island.
- **No minimize, no taskbar button, but in Alt-Tab.** Without a minimize
  button nothing needs the taskbar; Alt-Tab still finds it, with its
  provider's icon. No AppUserModelID per provider is needed.
- **Where it opens** (`placeWindow`): where the user left it, saved per
  provider as its screen and a frame in logical px from that screen's work
  area (so a scale change or a moved monitor keeps it in place); a
  resizable window keeps its size too. That screen gone, or never saved:
  by its `start`, on the island's screen. Always kept on a screen: whole if
  it fits, else its title bar's corner. Not reopened after a restart.

**The Windows side (next, needs a Windows run).**
- A borderless top-level window (no system title bar): `WM_NCCALCSIZE`
  takes the frame away, `DwmExtendFrameIntoClientArea` keeps Windows'
  shadow, `WM_NCHITTEST` gives `HTCAPTION` on the title bar (not over the
  ✕) and the resize edges when `resizable`. No `WS_MINIMIZEBOX`. Removed
  from the taskbar with `ITaskbarList::DeleteTab`, which leaves it in
  Alt-Tab. Windows 11 rounds its corners itself; Windows 10 doesn't, and
  the platform passes 0 for `windowCornerRadius` there. No Windows 11-only
  call is linked (decision 015).
- Its content is a Composition tree on the island's Compositor, with its
  own `DesktopWindowTarget`, drawn by the same element visuals.
- **Open and close grow out of and shrink into the pill on the island's
  spring** (decision 080), corners included: the spring runs in a
  short-lived transparent overlay covering the pill and the window's frame;
  at the end the real window, drawn the same, shows under it and the
  overlay goes a frame later, so nothing flickers. Decision 008's
  handoffs apply: check the timing on Windows.
- Each window is scaled for its own monitor and follows `WM_DPICHANGED`
  (decision 017 scales the island for its display only).
- Not yet: always-on-top windows (to need the user's permission), tearing a
  card off the island, a per-provider "open as" setting.

**Consequences.** `island.h` and `island.cpp` grow past ~300 lines; the
window rules live in `core/floating.h`, the scene in `core/window_layout.h`,
placement in `core/window_placement.h`. Rules are in
`tests/scenarios/floating.scenario`, the rest in `tests/window_tests.cpp`.
No provider asks for a window yet, so nothing changes on screen until the
Windows side lands.
