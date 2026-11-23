# 172: Statistics is an overview of tiles with a page behind each; measurement items

Status: Accepted (replaces 171's card, its laptop battery and its card-wide
high colour; 171's pull model, cleaning rules and 042 condition stand).
Core, layout and the provider tested. The renderer's side is written but
was never run: it needs a Windows build, and a look.

**Context.** 171's card was a table of three numbers with bars. The user
picked a design from a mockup instead: an overview of six tiles (CPU, GPU
and RAM as history graphs; network, the system drive, Bluetooth batteries),
each a button into a detail page inside the island, with a back arrow. No
laptop battery anywhere. The content model had nothing for a graph, a
per-core strip, a battery, a tile that is a button, a level that isn't a
time, a per-item "high" colour, or two columns in a card.

**Decision.**
- **Five new items, each one piece** (`core/stat_items.h`), like `Dial`:
  the model stays flat, no item holds items.
  `Graph`: samples 0..1 oldest first, optionally a whole tile (background,
  caption, detail and value over the plot), a dashed limit line, `alert`,
  and an `actionId` that makes all of it a button. `Levels`: a strip of
  upright bars with a label each (for a hover the host may add). `Battery`:
  the shape, optionally with a label before and a value after it.
  `LevelTile` and `BatteryTile`: the overview's disk and Bluetooth tiles;
  rows can't sit inside a tile, so each is an item.
- **Where their parts go is core's** (`core/stat_parts.h`): the plot and
  its line's points, each bar, the battery's body and fill, a tile's lines.
  The layout gives an item one frame; the renderer asks these functions for
  the rest and decides no geometry.
- **`Progress` takes a plain `level`** (the paused-`Timeline` trick is
  gone), a `second` part (memory: in use, then cached), `alert`, `thick`,
  and `stub` (a short fixed bar beside a value). **`alert` is per item**
  (`dangerColor`): one hot reading marks its own graph, bar or battery, and
  the card has no accent. A low Bluetooth battery is marked too.
- **`CardLayout::Columns`**: `expandedCenter` across the top (a page's
  header), `expandedLeading` under it at the left, `expandedTrailing`
  `sideColumnWidth` wide at the right, each a stack centred in its column.
  Two equal columns inside the left block are `Row::even` rows. One new
  layout, used by the four wide pages.
- **`Row::tileGap`**: a row of tiles sits `itemSpacing` under the one above,
  so a grid's gaps are one size and 124 + 8 + 80 fills the card exactly.
- **Pages are the card's own** (`Content::sizedLike` = the overview's rows,
  `backAction`, decisions 033, 070): the card keeps its height, Esc and the
  arrow go back. CPU, GPU, memory and disk are `wideCard` (a list beside
  graphs earns the width, decision 085); network and Bluetooth are not.
  Bluetooth's devices are a `Row::list` that fills the card.
- **The provider owns the view** (`StatsView`): the reading, the histories
  and the page. A page whose tile isn't there can't be opened (no GPU
  reading, no drives), and closes if its readings stop. Closing the card
  clears all three.
- **Histories keep gaps** (`stats_history`): `statsHistoryLength` readings
  per series, as read; a missing one is a hole in the line (a sample below
  0), never skipped (time would compress) and never repeated (a lie). A
  `Graph::capacity` makes a fresh history grow in from the right.
  Temperatures are drawn between `statsGraphMinTemperature` and
  `statsGraphMaxTemperature`; a speed against its own peak, at least
  `statsSpeedFloor`, so download and upload each fill their graph.
- **New fields go at a struct's end** (`Progress`, `Row`): the platform and
  tests build some positionally.

- **Drawn as pixels, not shapes** (`platform/stat_painting`): each of the
  five items, and a `Progress` with a `level`, is painted whole into its
  surface, as a Dial's middle is. They change every couple of seconds, not
  every frame, so a new reading is a repaint: nothing eases between two
  readings yet. A `Progress` without a level is still the shapes that follow
  a timeline on the compositor.
- **A tile that is a button is built like one**: the hover shape
  (`hoverColor`, a list row's, at the tile's corners) under its painted
  surface, showing through the tile's own translucent fill, and
  `pressScale` on press (art's gentle squash, not a small button's).
- **A plot is cut by its tile's rounded shape** with a Direct2D layer (a
  geometric mask), and so is a level in its bar: the area reaches the
  tile's bottom corners without painting past them.
- **Earbuds are drawn as headphones on Windows 10**: the glyph is in Segoe
  Fluent Icons only (decision 015).

**Not taken.** A generic container item (a tile holding rows): it would
make the model a tree. A window for the detail pages: the user asked for
them inside the island. A colour per `Content` for "high" (171): it turned
every bar red. The mockup's label on the big number's baseline ("58% Load"):
an even row gives each column one item, so the name is a caption row above
the number. A tile around the GPU's memory row and around earbuds' parts,
a circle behind a device's glyph, dots in memory's legend: nothing existing
draws them, and they weren't worth items.

**Consequences.** Ten glyphs are new (`Glyph::Headphones` .. `Ethernet`);
their code points were written from memory of Microsoft's tables and need a
look on both Windows versions. A bar's label under the pointer
(`LevelBar::label`) isn't shown: there is no tooltip on the island to hang
it on. A ring can't take a `level` (it still follows its timeline). In a
tile's value the unit is as bright as its number, and a caption sits on its
line's middle, not on the value's baseline. The reader's contract grew: per-core readings,
process lists, GPU engines and memory, cached memory, today's network
totals and link speed, every drive, disk activity, device kinds and parts.
Every one is optional, and each page was laid out without it. Process
lists are the costly part, and only three pages need them: the reader can
ask the provider which page is open (`StatisticsProvider::page`).
