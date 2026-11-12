# 084: Cards adapt to their size

Status: Proposed. Written for floating cards (081, 083), now gone; it applies
as written to resizable windows (decision 104), whose rows are the same.

**Context.** The content model (decision 004) and its presets (decision
014) were made for one card size: Expanded's, `expandedSize`, the same
for every card (decision 047). Rows of fixed-size items, stacked and
centred; only text and progress bars stretch with the width, and a
stacked card grows only as tall as its rows (decision 016). A floating
card that's resized (decision 081) shows the limits: Pomodoro at 760×900
was the same timer floating in more empty space. Web pages adapt to any
width because their layout is fluid (flex and grid, wrapping, min and max
sizes, container queries) and laid out again in the same frame. The
second half is the renderer's (decision 083's resize mode); this is the
first.

**Decision (proposed).** A few fluid rules in the content model, not a new
system. Providers say what may grow; the host decides how much, from
tokens (decision 014), and lays it out in core, tested on Linux.
- **Fill:** a row or a slot takes the room left over, across or down. A
  list already can (`Row::visibleRows < 0`, decision 070); the same for a
  Pomodoro card's to-dos in a tall card.
- **Wrap:** a row's items go onto a second line when the card is too
  narrow for them, instead of being squeezed or cut.
- **A readable width:** text rows stop at a maximum width and centre, like
  CSS `max-width`, so a wide card doesn't make long lines.
- **Sizes that follow the card:** a few items scale between their own
  smallest and largest with the card (Pomodoro's dial).
- **Arrangements by width, sparingly:** past a width a card's slots go
  side by side (Pomodoro's to-dos beside the timer instead of under it):
  container queries, chosen by the host; a provider never sees pixels.
- **Limits stay the provider's** (`FloatSpec::minSize`, `maxSize`): the
  smallest a card still makes sense at, and the largest worth filling.
  Pomodoro's were [400, 330] and [760, 900] (tokens removed with its
  floating card; they come back with it).

**Order.** 083 first (a resize has to be smooth before it's worth making
it adapt), then fill and the dial for Pomodoro, then wrap, the readable
width and side by side.

**Consequences.** New fields in `Row`/`Slot` and the item presets, each
optional, so existing providers don't change (decision 004: the model only
grows by adding fields). The island's cards keep their one size, so they
look the same; the rules only show on resized floating cards, and on
cards a future size change touches.
