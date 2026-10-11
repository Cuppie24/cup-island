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
- **Home.** Expanded gains a Home view: a grid of tiles, the clock first,
  then panels in the order they were published (stable, like tabs).
  `homeColumns` columns; a tile is one column wide or two (`Panel::wide`). Tiles that don't fit the card are not
  shown (a setting to choose them is open). Home keeps Expanded's card size,
  so switching between Home and a card doesn't resize the island.
- **Getting there.** Clicking the idle pill opens Home (when there are
  panels; with none, it still does nothing). A click on Home's empty space
  doesn't close it, unlike a card: Home is where things are started from,
  and a stray click between tiles shouldn't throw the user out. It closes
  like the rest of Expanded: outside click, Esc, idle timeout.
- **Tabs below, Home apart.** The tab strip (decision 007) runs along the
  bottom of the card, under its content: the card's content keeps its
  place whether or not the strip is there. With panels, Home is an
  icon-only button (`homeButtonWidth`) on its own at the strip's left edge;
  its click area takes in the card's bottom-left corner and half the gaps
  around it, so it's hard to miss. The same room is kept free on the
  right, so the activity tabs stay centred on the card.
  The strip shows when there's somewhere else to go: another activity, or
  Home from any card (including a dormant panel's with nothing running).
  Home with nothing running has no strip.
- **Static widgets.** Some panels have no card, only a tile that shows
  something: battery, the connected Bluetooth device and its battery. Their
  tiles don't react to the pointer and a click on them does nothing (it
  doesn't fall through and close Home either).
- **Toggles.** A panel can give its tile a *tap action* (`Panel::tapAction`,
  PS §7.2's "primary tap action"): a click anywhere on the tile sends it to
  the provider instead of opening a card, and the tile lights up under the
  pointer like any clickable one. The theme tile is the first: it shows the
  mode (moon "Dark" / sun "Light") and a click switches Windows between
  light and dark, apps and system together, as Settings does. It writes the
  Personalize values, tells running apps (`WM_SETTINGCHANGE`
  "ImmersiveColorSet", off the UI thread so a hung app can't stall the
  island), and watches the key, so a change made in Settings shows too.
  The island itself stays black in both modes. The clock tile (time, large,
  and the date) is built by the host from the clock it keeps for the idle
  pill, so nothing ticks it but the scene's minute deadline.
- **Tiles.** A tile's buttons route to its provider like any button (the
  dormant tile's ▶ starts it). A click elsewhere on a tile with a card opens it:
  the running activity's (and makes it the pick, like its tab), or the
  dormant panel's, where it can be started. On a dormant panel's card no
  tab is selected, so Home's button reads (and works) as the way back.
  (First tried: Home stayed selected there, as "part of Home"; a selected
  button looks like where you are, and clicking it did nothing.)
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
  of day (and the date, formatted for the locale, for Home's clock tile);
  core keeps the offset from its own clock and the scene asks for a
  re-layout at the next minute. The app re-reads the wall clock on every
  timer, so a clock or time-zone change shows within a minute. One wake-up
  per minute while idle is the only cost.

**Consequences.** Rules are in core (`IslandState::panels`, `homeOpen`,
`clock`) and `core/home.h` (tile grid), tested by
`tests/scenarios/home.scenario` and `tests/home_tests.cpp`. Pomodoro
(`providers/pomodoro/`) is the first panel and the reference for writing
one. Battery and Bluetooth (`providers/system/`) are the reference static
widgets: pure presenters, tested on Linux, fed by thin WinRT readers
(`PowerManager` events; device watchers, with the battery level Windows
keeps on a Bluetooth device's nodes, the one Settings shows). The renderer
draws a faint background under each tile, like a hovered tab.

**Open.** Which panels show on Home, and in which order, should be a
setting once there are more than fit; today the order is whichever panel
appears first (Bluetooth, published when a device connects, can land after
Pomodoro). Not every Bluetooth device reports a battery level to Windows;
those show "Connected". LE devices that only expose it over GATT aren't
read. Panels can't change their tile's size. A paused Pomodoro stays until stopped (no linger like media). Matched
geometry from tile to card (PS §3.4) isn't built.
