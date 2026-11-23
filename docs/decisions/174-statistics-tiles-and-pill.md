# 174: Statistics is one card of tiles, and a pill picked in Settings

Status: Accepted (supersedes 172: no graphs, no detail pages; amends 171:
it is an activity while its pill shows something; 173: the reading is cut
down and comes with a request, and the reader must follow). Core, layout,
the provider and Settings' page tested. The drawing compiles nowhere here
and was never run; the Windows reader follows the new contract (173,
rewritten), built only against mingw's headers here.

**Context.** 172's graphs needed a history, and a history only exists while
something samples: with the card closed there was none, so every graph
began empty. The user chose to do without graphs rather than sample in the
background, dropped the detail pages with them, picked a card of tiles
("bento"), and asked for the collapsed pill to show readings chosen in
Settings, Bluetooth devices among them, built to cost little: Bluetooth
once a minute is fine.

**Decision.**
- **One card, nothing to click.** A row of large tiles (CPU, GPU, RAM: a
  glyph and name, a detail at the right, the percent big, a level), and
  under it the disk over the network at the left and Bluetooth at the
  right, as tall as both. No GPU reading: two tiles on top. Standard width,
  the height every card has (100 + 8 + 48 + 8 + 48 fills it exactly).
- **Two items** (`core/stat_items.h`): `StatTile` (large, or small with or
  without a level) and `BatteryTile` (a row per device; one device alone
  gets the tile, its glyph in a circle; a note for none, or for "+N
  more"). Their parts' places are core's (`core/stat_parts.h`), painted
  whole by `platform/stat_painting`. Gone with 172: `Graph`, `Levels`,
  `Battery`, `LevelTile`, `Progress::level/second/alert/thick/stub`, tiles
  as buttons, the histories, every page.
- **`CardLayout::Columns` stays, redefined**: rows across the top, two
  equal columns under them, `columnGap` apart and below. The model is flat,
  so a tile as tall as two can only be a column beside a column.
  `Row::tileGap` stays for the two small tiles.
- **A new text style, `TextStyle::Stat`** (28 at 800): the card's readings
  sit between `CardTitle` and `Hero`. Its own colour is the caption's and
  it is drawn with bright digits, so a "%" beside a number is quiet.
- **Four glyphs are drawn, not typed** (`platform/stat_glyphs`): a
  processor, a graphics card, memory, the network's arrows. Segoe MDL2
  Assets has no glyph for the first three; lines from the design look the
  same on Windows 10 and 11 (decision 015).
- **The Bluetooth tile stays when nothing is connected** ("No devices
  connected"): the grid doesn't rearrange when headphones connect. More
  devices than lines: the emptiest, and "+N more" on the last line.
- **The network tile has no name**: both speeds with their unit take the
  line, and a caption that came and went as the numbers grew would
  flicker. A caption that doesn't fit is left out whole, never cut.
- **The pill shows up to `statsPillReadings` (2) picked readings**, in the
  card's order: CPU load, CPU temperature, GPU load, memory, disk, network,
  and any known Bluetooth device's battery. The CPU's two share a name
  ("CPU 14% · 46°"). A pick with no reading (no temperature on this PC, a
  device not connected) is left out, not shown as a dash; if none has one
  there is no pill. Two of the fixed readings always fit; a pair with a
  device's name may swim (`Text::marquee`).
- **Turning on one more than fit turns off the one picked longest ago.** A
  switch always answers; refusing the third would need a reason shown.
- **With a pill to show, statistics is an activity** on its panel's id,
  `Level::Passive` with relevance 0: under media and the timer whatever
  started last (PS §6.2). Beside a song it is the bubble; with a song and a
  timer it shows nowhere. Without a pill it is the dormant page of 171.
- **Settings lists, statistics rules.** A Statistics page in Settings'
  sidebar: a switch per reading and per Bluetooth device seen. The picks,
  their rule and their saving (`Host::save`, key `pill`) are the statistics
  provider's; Settings reaches them through two hooks the app wires
  (`listStatistics`, `toggleStatistic`), as it reaches displays and
  hotkeys, and is told when the list changes by itself
  (`statisticsChanged`). No provider knows the other. Devices are known by
  name, the latest `statsKnownDevices` (8) seen, kept in the same text.
- **What is read, and when**, is the pure provider's, and goes to the
  reader with each request (`StatsRequest`: a flag per reading, and
  `bluetooth` when the devices are due; it replaces `page()`):
  - card shown: everything, every `statsRefresh` (2 s); Bluetooth every
    `statsBluetoothRefresh` (10 s);
  - card closed, readings picked: only those, every `statsPillRefresh`
    (5 s); a picked device every `statsPillBluetoothRefresh` (60 s);
  - only a device picked: one wake-up a minute, on the wall clock's
    minute, where the idle island wakes for its clock anyway (TR §8);
  - card closed, nothing picked: nothing, and what it had is forgotten.
  A reading that didn't list devices (`bluetooth` empty) keeps the last
  listing.

**Not taken.** Hiding the Bluetooth tile with no devices and widening the
other two: the card would change shape with a headset. Pins on the tiles
to pick the pill's readings: Settings only, as asked. A "New" tag for the
Statistics page (decision 048): a tag is for a row added to a page people
know; a new page shows in the sidebar by itself. Greying a switch whose
reading this PC lacks: it is only known after a full reading, and the pill
already leaves it out.

**Consequences.** With a reading picked the app wakes every 5 s for as
long as it runs (a counter read on the reader's thread, a repaint of one
line): the cost of a live pill, and why nothing is picked by default. A
picked device costs one Bluetooth listing a minute, under 042's condition
and still to be tested with music playing. A device renamed in Windows is
a new device to the list. Tokens: the pill's three intervals and three
counts, and the tiles' sizes, replace 172's graph, level and history ones.
The reader's contract shrank to what is shown: no per-core values, names,
processes, engines, GPU memory, cached memory, link, extra drives or disk
activity; `cpuCores` and `diskUsed/Total` are new.
