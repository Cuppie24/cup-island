# 171: A statistics page; readings asked for only while it's shown

Status: Accepted for the pure half (the contract, formatting, the card,
the provider's asking); tested. The Windows reader isn't written, and the
provider isn't registered in `app/main.cpp` yet. Amends 042: Bluetooth
battery levels may come back, under its condition.

**Context.** The user asked for a page of the machine's statistics: CPU,
GPU and memory load, temperatures, Bluetooth devices' batteries, and the
rest (disk, network, the laptop's battery). On Windows much of that is
often unreadable (a temperature needs admin rights or a vendor's API), and
reading it costs something: decision 042 removed Bluetooth entirely because
a standing device watcher made music stutter.

**Decision.**
- **A page, never an activity.** `StatisticsProvider` publishes one panel
  (`statistics.system`, `Glyph::Gauge` in the page row) and never starts an
  activity: nothing in the pill, no compact or minimal slots, no alerts.
- **Pull, not push.** The provider asks for a reading when its card is
  shown and every `statsRefresh` (2 s) after, through `Host::wakeAt`; closed,
  it asks for nothing. The reader is a callback (`RequestReading`) that
  returns at once and answers later with `publish(StatsReading)` on the UI
  thread (decision 144). So no watcher or timer outlives the card, which is
  042's condition for Bluetooth: the reader must list devices once per
  request and hold nothing open between them.
- **Closed, the card goes back to "—".** Opened an hour later it must not
  show an old load as now's. A reading that arrives after closing is
  dropped; one that gives the same card publishes nothing.
- **Every value optional; the reader passes on what it got.** Cleaning is
  in one tested place (`stats_format`): a percent that isn't a number is no
  reading, otherwise clamped to 0..100; a temperature is never clamped (a
  made-up 125 °C would read as real): not a number, 0 or below, or past
  `statsPlausibleTemperature` is no reading.
- **What's missing keeps its place or loses its row.** A tile's percent is
  "—" over an empty bar and the details keep their row even when all are
  empty, so the tiles never move; the row of the rest and the Bluetooth row
  show only what was read, and go when nothing was.
- **Bluetooth: only devices with a battery level**, the emptiest first (the
  one to charge), by name among equals; with more than
  `statsBluetoothShown`, the last place says "+N more". A name is cut to
  `statsDeviceNameMaxLength` by the provider, so the "…" never eats the level.
- **High is judged on the number shown** ("90%" is high, never "89%"):
  `statsHighLoad`, `statsHighTemperature`. Anything high turns the card's
  accent to `dangerColor`: every fill, since an accent is the card's
  (decision 010). Which reading is high is read from its number.
- **Thresholds are whole-number tokens** (`std::size_t`): float tokens are
  lengths, multiplied by the display's scale.
- **Units** are binary (GB as Task Manager counts); a speed never shows
  more than three digits, and under 1 KB/s is "0 KB/s".

**Not taken.** A ring per tile with its percent inside: `Dial` shows only a
time, and a `Progress` ring has no text; no new item was added for it. A
colour per bar, or a low battery in red: there is no per-item colour. A
header ("System"): the lit page-row icon already says which page it is.
The reader pushing on its own timer: it would run with the card closed.

**Consequences.** The card is built from rows of equal columns
(`Row::even`), one function per row (`stats_card`), so it can be rearranged
to a mockup without touching formatting. A low Bluetooth battery isn't
marked, only sorted first. Six `stats*` tokens and `statsRefreshMs`. The
Bluetooth part needs 042's test on Windows: music over Bluetooth headphones
with the card open.
