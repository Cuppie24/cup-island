# 009: Panels, Home and the idle clock

Status: Accepted

**Context.** Built-ins like Pomodoro, calendar and battery should always be
reachable, even when nothing is running, without sitting in the pill or the
bubble. Before this, an activity was either live (ranked into the pill,
bubble and tabs) or didn't exist, and Expanded always showed an activity, so
nothing could be started from the island. The idle pill also showed nothing.

**Decision.**
- **Panels.** A provider can publish a *panel* (`Host::publishPanel`): an
  always-available entry with a **tile** (`Content::tile`, a small glance)
  and a **card** (the `expanded*` slots). A panel is not an activity: it is
  never ranked and never takes the pill or the bubble.
- **Shared id.** A panel turns into an activity by starting one with the
  panel's id. While it runs, its tile shows the activity's `tile` slot (falls
  back to the panel's) and its card is the activity's card. When the
  activity ends, the panel is dormant again. A *launcher* is just a dormant
  panel whose tile has a start button; no separate concept.
- **Home.** Expanded gains a Home view: a grid of tiles in the order panels
  were published (stable, like tabs). `homeColumns` columns; a tile is one
  column wide or two (`Panel::wide`). Tiles that don't fit the card are not
  shown (a setting to choose them is open). Home keeps Expanded's card size,
  so switching between Home and a card doesn't resize the island.
- **Getting there.** Clicking the idle pill opens Home (when there are
  panels; with none, it still does nothing). With panels, the tab strip
  (decision 007) gets a Home tab first, then a tab per activity. The strip
  shows only with two or more tabs.
- **Tiles.** A tile's buttons route to its provider like any button (the
  dormant tile's ▶ starts it). A click elsewhere on a tile opens its card:
  the running activity's (and makes it the pick, like its tab), or the
  dormant panel's, where it can be started. A dormant panel's card counts as
  part of Home: the Home tab stays selected.
- **Exits.** Ending is the provider's: `endActivity` at a natural end (media
  after the paused linger, Pomodoro after its last break), or a stop button
  it puts on its tile and card. What "stop" means differs per provider, so
  the host doesn't add a generic one. Ending while its card is open shows
  the dormant panel's card instead of collapsing.
- **Provider wake-ups.** Providers that change by themselves on a schedule
  (a phase ending) ask `Host::wakeAt(t)` and get `Provider::onWake()`. The
  registry keeps the times; the app folds `nextWake()` into its one timer,
  so nothing polls (TR §8).
- **Idle clock.** The idle pill shows the local time ("14:05", or "2:05"
  with a 12-hour locale, no AM/PM: it has to be glanceable in a small pill).
  Core can't read clocks, so the platform sends `ClockSynced` with the time
  of day; core keeps the offset from its own clock and the scene asks for a
  re-layout at the next minute. The app re-reads the wall clock on every
  timer, so a clock or time-zone change shows within a minute. One wake-up
  per minute while idle is the only cost.

**Consequences.** Rules are in core (`IslandState::panels`, `homeOpen`,
`clock`) and `core/home.h` (tile grid), tested by
`tests/scenarios/home.scenario` and `tests/content_tests.cpp`. Pomodoro
(`providers/pomodoro/`) is the first panel and the reference for writing
one. The renderer draws a faint background under each tile, like a hovered
tab.

**Open.** Which panels show on Home, and in which order, should be a
setting once there are more than fit. Panels can't change their tile's
size. A paused Pomodoro stays until stopped (no linger like media). Matched
geometry from tile to card (PS §3.4) isn't built.
