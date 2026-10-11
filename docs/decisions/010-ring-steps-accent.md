# 010: Progress ring, steps, and accent colour

Status: Accepted

**Context.** Pomodoro wants a ring filling in the compact pill, the phases
passed in its card, and a colour per phase (focus vs break). Providers
can't draw (PS §5.3), so each of these is a host-drawn item or property.
PS §5.2 already plans `progress` with `shape: bar | ring` and an `accent`.

**Decision.**
- **Ring:** `Progress::shape = Ring`. A square that fills its slot's height,
  like art: the compact squares, the Split bubble, Expanded's leading
  square. A faint full circle under an arc that starts at 12 o'clock and
  runs clockwise. It fills from the `Timeline` on the compositor (the
  arc's `TrimEnd`, armed once per change, like the bar's width), so a
  running timer costs no app CPU and sends no updates. Never seekable.
- **Steps:** a row of dots, one per step: done ones white, the current one
  in the accent, the rest faint. Fixed width (`stepDotSize`,
  `stepDotGap`). For Pomodoro's phases; also fits any multi-step task.
- **Accent:** `Content::accent`, an optional colour. It tints only the
  activity's *live data*: progress fills (bar and ring), the current step,
  waveform bars. Text, icons and buttons stay white, and the island stays
  black. A tinted island would fight whatever is on screen and stop reading
  as part of the screen edge; colour on the data alone is enough to tell
  focus from a break at a glance, and stays calm with several activities
  side by side. Layout stamps it on each `Placed`; no accent = white.
- **Countdown labels** are centred in their frame and take a text style,
  so a countdown can be the main thing in compact (Body) instead of a
  dim caption. `Text::centred` carries the alignment to the renderer.

**Pomodoro** uses them: compact is the timer icon, the countdown centred,
the ring on the right, with no phase name (the colour says it); the
bubble is the ring; the card has a big ring, the phase name, a dot per
phase (focus, break, …, long break) and "n of 8 phases done". Focus is
tomato red, breaks green.

**Consequences.** Rules in core (`slot_layout.cpp`), tested by
`tests/item_tests.cpp` and `tests/pomodoro_tests.cpp`. The renderer
(`element_parts`: `createRing`/`armRing`; `paintSteps`) needs a Windows
run to check the arc's start and direction.

**Colours are tokens.** Every colour the island draws is in tokens.json,
as `[r, g, b]` or `[r, g, b, alpha]` (channels 0..255, alpha 0..1, like CSS
`rgba()`): the island, text per style, icons, buttons, tracks and fills,
tabs and tiles, steps, and Pomodoro's phase accents. A reload rebuilds the
content visuals, so new colours show at once.

**Open.** Accents aren't contrast-adjusted (PS §5.2 asks for it once there's a light
theme). Indeterminate progress, button styles and the other items listed
in the content-vocabulary discussion come when a provider needs them.
