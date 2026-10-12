# 019: Every provider gets a widget; widgets only inform

Status: Accepted

**Context.** Home (decision 009) had grown into a mix: a clock tile with
the date, a battery tile, and a Pomodoro tile with its own ▶ / ■ buttons.
Small buttons on small tiles were easy to mis-hit, a tile meant different
things depending on where it was clicked, and the media player, the thing
used most, wasn't on Home at all unless something was already playing.

**Decision.**
- **A widget per activity provider.** An activity provider shows on Home as
  a widget: its panel's tile (decision 009), there whether or not anything
  is live. Media and Pomodoro come first (they're started first), then the
  system tiles (theme, Bluetooth, Settings: decision 022).
- **Information only.** A widget shows state, never controls. The host
  leaves out any `Button` or `TextInput` a tile slot carries, so a click
  anywhere on a widget means one thing: open its card, where the controls
  are. Toggles (the Windows theme) keep their tap action: the whole tile is
  the control.
- **A leading square.** `Content::tileLeading` is a square at the widget's
  left, as tall as the tile's inside (art, an icon, a ring), with the
  `tile` rows beside it: the same shape as Expanded's Standard card, so a
  widget reads like a small card.
- **Media.** The media panel (`media.nowPlaying`, the activity's id) is
  published when the provider starts. Idle: a music note, "Not playing",
  and a card that says music from any app shows there. Playing: the art,
  the title (swimming when long) with the equalizer bars, the artist.
- **Pomodoro.** Dormant: the timer icon, the focus time ("25:00"), the
  plan. Running: the ring filling in the phase's colour, the time left,
  the phase and round ("Focus · 2/4", "Paused · 2/4"). Starting, pausing,
  skipping and stopping are on its card.
- **Every row full width, nothing hidden.** Widgets fill rows in publish
  order, `homeColumns` columns' worth per row (a wide widget counts two);
  one that doesn't fit a row's rest starts the next. Each row then
  stretches to reach both sides of the card: its widgets share the width
  by span, so a row that isn't full never leaves a hole (two narrow
  widgets alone are halves). No widget is left out: Home's card grows to
  fit every row, up to the canvas, like a Centred card (decision 016).
- **No clock or battery tiles.** The idle pill already shows the time;
  the date and the battery level are one glance away in the taskbar.
  `ClockSynced` no longer carries a date.

**Consequences.** Rules in `core/home.cpp` (`informationOnly`, the leading
square, full-width rows, `homeCardHeight`), tested in `tests/home_tests.cpp`; the widgets in
`tests/media_tests.cpp` and `tests/pomodoro_tests.cpp`. A provider that
wants a one-click action on Home uses a tap action, not a button. The
demo music player (M key) isn't a panel, so it has no widget.
