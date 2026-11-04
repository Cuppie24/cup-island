# 081: A card can float apart from the island

Status: Accepted for the rules, the app's surfaces, placement and
resizing. The first Windows drawing below (a window pair per card, a flow
handed off between windows) is superseded by 083 (Proposed): it flickered
opening and resizing. Until 083 lands no provider floats a card; Pomodoro
is back on the island's card. Fluid layout for resized cards: 084.

**Context.** A card grows out of the island and covers the top of the
screen while it's open; a click elsewhere or leaving it alone closes it,
and even pinned (decision 046) it stays where the island is. Some cards
are worth keeping beside other work, somewhere the user picks: a timer, a
to-do list. Several providers may want this at once.

The first idea was a regular window (title bar, Alt-Tab, behind other
apps). It was dropped:
- What's inside is still a card: slots of rows, laid out by the host
  (decisions 004, 014). A regular window invites richer UI, and growing the
  content model into a UI toolkit is what decision 003 avoided.
- A "regular" window without minimize, taskbar or the system title bar
  behaves irregularly, and Windows' frame is a different object from the
  island's shape, which the morph is all about.
- A third-party plugin that needs a real app window (PS §8, Tier 2, its own
  process) should draw it itself; the island would only bring it to the front.

**Decision.** A **floating card**: the card, in the island's look, left
wherever the user drags it, while the island keeps showing its pill.
- **The provider asks, the host decides how** (decision 014):
  `Content::floating` (a `FloatSpec`): a width class (`floatNarrowWidth`,
  `floatMediumWidth`, `floatWideWidth`), `resizable`, where it first opens
  (`NearIsland`, under the pill, or `Centre`), and `escCloses`.
- **Its look** (`floatScene`): like a window, from the first run: a
  title bar strip (`floatTitleBarHeight`) with the ✕ inside its right end,
  over the card laid out as Expanded lays it out; a window's corners
  (`floatCornerRadius`, as Windows 11 rounds them), not the island's; its
  own shadow. The first version had no title bar and a ✕ half outside the
  corner on hover, with the island's big radius: it read as a stray piece
  of the island, not a window. Its title bar and empty space drag it.
- **Its size:** as tall as its content plus the bar, like the island's
  cards (decision 016). A resizable one (`FloatSpec::resizable`) resizes by
  its edges and corners (a `floatResizeBorder` band, the resize cursors),
  between `FloatSpec::minSize` and `maxSize` (else `floatMinSize` and
  `floatMaxHeight`, `floatLimits`), follows the pointer at once, and opens
  at the size it was left at. Pomodoro's resized between [400, 330] and
  [760, 900] while it floated (those tokens went when it stopped, decision 084).
- **Above other apps**, like the island (the island stays above the
  floating cards). No taskbar button, not in Alt-Tab, no minimize: it can't
  get lost behind other windows, so nothing needs to find it.
- **The pill, the bubble and the activity shortcut toggle it**: open, or
  back into the pill (as the shortcut toggles a card, decision 050). A tab,
  a Home widget or an alert only bring it to the front of the other
  floating cards. The island goes back to its pill when one opens, unless
  it's pinned open (decision 046). They open only from these user actions.
- **One per card, several at once, from several providers**, with no limit:
  each takes a click, and there's one per card.
- **Focus decides where keys go**, and the focus is Windows': the platform
  reports `FloatFocused` on `WM_SETFOCUS` / `WM_KILLFOCUS` for every
  floating card and for the island's own window, and keys, typing, Esc,
  and answers to a dialog or toast go to that card (`focusedCard`). Core
  sets it when it opens one, so a key arriving before Windows says so finds
  it. One `Draft` and one list drag at a time; a card losing the keys keeps
  its field's text (decision 050).
- **One `CardOverlays` per shown card** (its dialog and toast): the island's
  card has one (`IslandState::card`), each floating card its own, found by
  `overlaysOf`. A question can wait on one while the user works in another.
- **Sending it back:** the pill, the ✕ (host-owned action `kCloseFloat`),
  Alt+F4 while it has the keys (unlike the island, decision 049), Esc once
  the card has no step back (`Content::backAction`, decision 033) unless
  `escCloses` is false, its provider closing its card (`Host::closeCard`),
  or the card going away. An activity ending with a panel behind it leaves
  the dormant card, as on the island (decision 009). Sending it back drops
  its visuals; the provider gets `onCardClosed` and keeps its own state.
- **Hidden with the island** while a fullscreen app is in front (they float
  above apps too, so they'd cover the game), back when it is; a critical
  alert leaves them.
- **Providers barely change:** card callbacks already come per activity, so
  `onCardShown` / `onCardClosed` fire for several cards at once
  (`shownCards`, `cardChanges`).
- **Where it opens** (`placeFloat`): where the user left it, per card, else
  where its provider's last one was (a card with a new id each time); saved
  as its screen and a frame in logical px from that screen's work area, so
  a scale change or a moved monitor keeps it in place. That screen gone, or
  never saved: by its `start`, on the island's screen. Kept on a screen:
  whole if it fits, else its top-left corner. Never exactly on another
  open one: a `floatCascade` step right and down. Not reopened after a restart.

**The Windows side** (first version, superseded by 083: see there for what went wrong).
- The app has **surfaces** (`app/surface.h`): a scene, its renderer, its
  interaction window, and the pointer on it. The island is one; each
  floating card another (`app/floating_cards.h`, kept in step with
  `IslandState::floats` after every dispatch); every input handler takes
  the surface it came from, and keys go to the one with the keys.
- Each floating card is a pair of windows, like the island (decision 001;
  `platform/float_window.h`): a layered, click-through render window a
  shadow's width bigger than the card, on the island's Compositor; and an
  input window over the card, its region the card's rounded shape plus the
  ✕, so clicks beside it reach what's under it. Both move together
  (`DeferWindowPos`) while dragged. Topmost tool windows (no taskbar, no
  Alt-Tab), not `WS_EX_NOACTIVATE`: clicking it gives it the keys. The
  island is put back on top after one is raised.
- Drawn by the island's own renderer in a floating mode
  (`PillRenderer::buildFloating`): one shape, detached (`Blob::detach`), its
  ✕ in an unclipped layer (`Blob::setChrome`), so it looks and reacts
  exactly like the island's card.
- **Pomodoro's card floated** (both its dormant and running cards), the
  first to; it's back on the island until decision 083.
- **It flows out of the island** (`platform/float_flow.h`): a copy of the
  pill, in the island's colours, peels off the top edge as the island lifts
  (decision 026: the ears fold, then the corners round) and springs on the
  island's spring (decision 080, growing overshoots) into the card's place,
  its corners easing to a window's. It runs in one click-through layer over
  the pill and the card, under the island, so it comes out from under the
  pill; then the card's own windows show and its content fades in, and the
  layer goes `floatHandOffMs` later, so nothing blinks (decision 008).
  Going back is the same the other way. Reduced motion: no flow.
- **For now** it's laid out at the island's display scale on every
  monitor; per-monitor scale is next.
- Not yet either: tearing a card off the island by dragging it, a dot on the
  pill while its card floats, holding back an activity's alerts while its
  floating card has the keys.

- **Windows are changed after a dispatch, never inside it**
  (`app/floating_cards.cpp`, `applyWanted`): each dispatch copies what it
  wants on screen, and a queued pass makes the windows match. Raising a card
  gives it the keys, which tells the island at once (`WM_ACTIVATE`: a click
  outside) and dispatches again; inside the first dispatch, that replaced
  the state the loop was reading, which crashed the first build that
  floated Pomodoro.

**Consequences.** `island.h` and `island.cpp` are past ~300 lines; the
rules live in `core/floating.h`, the scene in `core/float_layout.h`,
placement in `core/float_placement.h`. Rules are in
`tests/scenarios/floating.scenario`, the rest in `tests/float_tests.cpp`.
Tests that look at a card's content use `tests/card_scene.h`, which lays
it out wherever it's shown.
