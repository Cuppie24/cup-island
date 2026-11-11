# 090: Providers get windows of their own, drawn by the host

Status: Accepted for the core, the Host API and the registry (built and
tested). Proposed for the window's layout and its Windows side (next).
Supersedes decision 081's floating cards and decision 083's canvas; 081's
placement stays, as window placement.

**Context.** Some things don't belong on the island: Settings, which keeps
growing; a timer's full view (statistics, to-dos, its settings) while the
island shows it compact; later, a plugin's own screen. Decision 081 tried a
*floating card*: the card in the island's look, torn off and left above
other apps, kept in `IslandState`. It went wrong three ways:
- **It's an app window that pretends not to be.** No taskbar button, no
  Alt-Tab, always above other apps: fine for a tiny timer, wrong for
  Settings or a full view, which people expect to behave like windows.
- **It tied windows to the island's state machine.** `IslandState` grew
  `floats`, a focused card, raise counters, overlays per card; `island.cpp`
  went past 450 lines; every rule had a "floating?" branch.
- **It reused the island's rendering, built for a morph.** The fixed,
  click-through canvas with an input window over it (decisions 001, 026)
  flickered as soon as a card's window moved and resized (decision 083).
A card is also the wrong unit: slots sized for a pill and a card, not
pages with sections, a sidebar, a footer.

The other ways were weighed: a separate C# (WinUI 3 / WPF) app over the
pipe (decision 003's fallback): real controls, but a second toolchain and a
second look; XAML Islands in-process: needs Windows 10 1903+, against
decision 015's 1809; WebView2: heavy and not on every Windows 10; a plugin
drawing its own window: allowed as a last resort (below), never the default.

**Decision.** A provider describes a window as data, as it does a card;
the host lays it out, draws it as an ordinary window, and sends its clicks
back. Windows share the *platform* around the island, not the island.

- **What's in one** (`core/window_content.h`): `WindowContent` is a title,
  an accent, pages (`WindowPage`: id, title, glyph, badge) of sections
  (`WindowSection`: an optional heading over rows), and an optional footer
  pinned under the pages. The rows are the island's own `Row`s and items
  (content.h): text, buttons, switches, fields, lists that scroll and
  reorder, row menus, the dial, tables. One page: no sidebar; several: a
  sidebar. New items are added to `Item` for both, reviewed once (PS §5.3).
- **How it's asked for** (`WindowSpec`): an id (the provider's own name,
  one window per id), a size class (`windowSmallSize` … `windowLargeSize`;
  the host owns sizes, decision 014), `Centre` or `NearIsland`,
  `resizable`, `keepOnTop` (a timer kept in view), and a page to show.
- **The Host API** (`core/provider.h`):
  `openWindow(spec, content) → WindowOpen` opens it, or replaces an open
  one's content and raises it; `updateWindow(id, content)` changes it
  without raising (the user's page, scroll and typing stay);
  `closeWindow(id)`. Its clicks reach `onAction` with `Action::windowId`
  set; picking a page sends `Action{kWindowPage, text = page}`; the user
  closing it calls `onWindowClosed(id)` (not when the provider closed it).
- **Not the island's state.** Windows aren't ranked, timed out or hidden
  with a fullscreen app. The open ones (`Windows`, `core/open_windows.h`)
  are kept by the `ProviderRegistry`, beside `IslandState`, and changed by
  pure functions; their rules are in `tests/scenarios/windows.scenario`.
- **Only right after the user acts on the provider**
  (`windowOpenGrace`): a click routed to it or its card opening. Opening
  or raising a window at any other time is refused (`WindowOpen::NotNow`),
  so nothing pops up or takes the keys by itself (PS §9: providers can't
  take focus). Updating and closing need nothing. Built-ins and plugins
  alike (PS §1, rule 3). At most `windowsPerProvider` open per provider;
  ids are letters, digits, `-`, `_`, `.` (they name files and keys).
- **A provider stopping closes its windows** and forgets its last action.
- **Where it opens** (`core/window_placement.h`, 081's placement): where
  the user left it (per window, else its provider's last), in logical px
  from its screen's work area; else by its `start`; always on a screen,
  cascaded off another one in the same spot. A resizable one reopens at the
  size it was left at, never under `windowMinSize`.

**What's shared, what isn't.**
- Shared: the Host API and the registry's routing; tokens, themes and
  display scale; the rows and items, their layout helpers (`slot_layout`,
  lists, text wrap), their painting (`element_painting`, `graphics_device`,
  DirectWrite), text editing (decision 043), keyboard focus (decisions
  031, 035), row menus (086), dialogs and toasts (020, 071).
- Not shared: `IslandState` and `reduce()`, slots and modes, `PillRenderer`
  and its shapes, the spring (080), the render and input window pair
  (001, 026). A window is one ordinary window.

**Next, proposed.**
1. `windowLayout(window, size, measure) → Scene`, pure: sidebar, page
   title, sections, footer, scrolling; and each window's own interaction
   state (focus, draft, scroll, list drag) with its reducer, in core.
2. The Windows window (`platform/provider_window`): a normal top-level
   window (title bar, taskbar, Alt-Tab; topmost with `keepOnTop`), a
   Composition target on the island's Compositor (`createWindowTarget`),
   the elements drawn by the same painters. Win11's rounded corners and
   Mica looked up at runtime with a fallback (decision 015). It must resize
   without flicker before anything ships in it: content laid out in the
   size message, anchored top-left, the window's background the theme's.
   If Composition can't keep up with a live resize, a flip-model swap chain
   drawn in `WM_SIZE` instead; that's the prototype's question.
3. Pomodoro's full view (the card keeps its compact role), then Settings.
4. Plugins (PS §8 Tier 2) send `WindowSpec`/`WindowContent` as JSON over
   the pipe; a preview tool renders a JSON file and reloads it on save.

**A plugin's own window**, for what the content model can't show, stays
possible and needs no host API to launch anything: the host starts only
the helper named in an enabled plugin's manifest (decision 078) and never
runs a path a provider gives it. Bringing that helper's window forward
would use the same rule as `openWindow` (only after a click on its card,
only for the helper the host started).

**Consequences.** Floating cards are gone: `Content::floating`,
`FloatSpec`, the float events, `IslandState::floats`, `core/floating`,
`core/float_layout`, `platform/float_window`, `platform/float_flow`,
`app/floating_cards`, the renderer's floating mode and the blob's
detach/flow, the `float*` tokens. The island's overlays go back to its one
card (`overlaysOf`, `cardOnScreen`, `shownDialog` in card_overlays.h).
`Display::workArea` and `createWindowTarget` stay for windows. Nothing
opens a window yet: no provider calls `openWindow` until the window is drawn.
