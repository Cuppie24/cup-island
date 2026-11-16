# Design rules

The sizes, weights and spacing content uses on the island. Values are tokens
(`assets/tokens.json`); this page says which to reach for. Why each is the way
it is lives in the decision cited.

## The card scale is the default (decision 111)

Media's and the timer's cards (decision 107) set the scale: bigger, heavier
pieces with more room between them. Anything people glance at on the island,
a card or a Home widget, uses it unless a rule below says otherwise.

| Piece | Use | Size |
| --- | --- | --- |
| A card's main line (a song, a phase) | `TextStyle::CardTitle` | 20 px at 800 |
| A Home widget's first line (a song, a time, its name) | `TextStyle::TileTitle` | 17 px at 700 (decisions 114, 119) |
| The line under it (artist, phase and round, a widget's detail) | `TextStyle::Body` | 15 px at 600, `bodyTextColor` |
| A third, quieter line (the app, "Track 3 of 12") | `TextStyle::Caption` | 13 px at 600 |
| The one thing a card is about (the timer's time) | `TextStyle::Hero` | 56 px at 800 |
| The card's one main action (▶/⏸) | `Button::primary` | 50 circle, 22 px glyph (44 and 18 `compact`, where a row is short) |
| The controls beside it (previous/next, stop/skip) | `Button::large` | 40, 20 px glyph |
| Less-used controls (shuffle, repeat, the window's button) | `Button::secondary` | 36 (32 `compact`) |
| A widget's leading icon | set by Home (`Icon::large`) | `tileIconSize` 20 |

Spacing:
- A card's row of controls is `loose` (`looseItemSpacing` 24); groups of rows
  on a stacked card sit `apart` (`groupGap` 20). Spare height goes between
  groups, never as a band at the bottom (decision 107).
- Home's widgets: `tileHeight` 64, `tileGap` 10, `tilePadding` 8, so a
  TileTitle line and a Body line fit with room around them. A widget is a
  little under its card: a glance at many, not one.
- Every card and Home share `expandedSize.y` (248) so switching tabs doesn't
  resize the island; a test pins media, the timer and Home to it.

Decision 119 took the card scale a step down (the cover 108, steps 30 and 14
long): the pieces read a little smaller in the same 248 height, with more
room between them. A Standard card's bottom rows share the spare height
evenly above each, so media's bar sits halfway between the cover and its
controls.

## Standalone windows: a step up, under the cards (decision 112)

Windows (Settings, the timer's) hold pages of rows to read at a desk, so
they sit between the island's lists and its cards. Providers write the plain
styles; the window's layout swaps in its own step, so nothing to remember.

| Piece | On the island | In a window |
| --- | --- | --- |
| `Title` | 17 at 700 | `WindowTitle` 18 at 700 |
| `Body` (a setting's name, a sidebar entry) | 15 at 600 | `WindowBody` 16 at 600 |
| `Caption` (its value under it, a subtitle) | 13 at 600 | `WindowCaption` 14 at 600 |
| A group's label (`Heading`) | | 15 at 600 |
| A page's name (`PageTitle`) | | 26 at 800 |

Room (window-only tokens): a grouped row 11 above and below its content
(`groupItemPadding`), groups 16 apart (`sectionGap`), their corners 14; the
sidebar 200 wide, entries 40 high; a switch 40 × 22, segments 32, key caps 26.

A page of cards (`WindowPage::cards`, decisions 117, 120), the timer's task
groups: each card at least `windowCardMinWidth` (440) and sharing the row,
one background with its rows `windowCardPadding` in; its name a `Heading`
lined up with the checkboxes, its rows plain (no list item's padding), 36 apart.
A rail sidebar (`WindowContent::rail`) is `windowRailWidth` of glyphs.

## Where the dense scale stays

Lists hold many rows to scan, not one thing to glance at; on the island they
keep the smaller scale: `Title` (17/700), `Body`, `Caption`, `buttonSize` 36,
`compact` buttons in rows.
- Search's results (`Result`, `ResultDetail`; decision 079).
- Grouped rows, menus, dialogs (decisions 020, 086, 090).
- Alerts (the banner that drops in): `Title` and `Body`, like every alert.
- Menus, dialogs and toasts (decisions 020, 086).
- Text inside a widget's rows: an inline icon stays `iconSize` (18), so the
  row is as tall as its text.

## Colour and weight

- Three weights of control (decision 094): the filled primary, bright
  buttons, quiet secondaries. Widget icons are quiet (decision 073).
- Values bright, the words around them quiet (decision 068): a time's digits
  in the title colour, its ":" in the caption colour.
- An accent tints only live data: fills, the current step, waveform bars;
  never text or icons (decision 010).

## Adding a piece

Reach for an existing style or flag first. A new size is a token in
`core/tokens.h` and `assets/tokens.json`, never a literal, and gets a row in
the table above.
