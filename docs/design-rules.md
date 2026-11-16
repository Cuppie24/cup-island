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
| A card's or widget's main line (a song, a time, a widget's name) | `TextStyle::CardTitle` | 22 px at 800 |
| The line under it (artist, phase and round, a widget's detail) | `TextStyle::Body` | 15 px at 600, `bodyTextColor` |
| A third, quieter line (the app, "Track 3 of 12") | `TextStyle::Caption` | 13 px at 600 |
| The one thing a card is about (the timer's time) | `TextStyle::Hero` | 64 px at 800 |
| The card's one main action (▶/⏸) | `Button::primary` | 56 circle, 22 px glyph (48 and 20 `compact`, where a row is short) |
| The controls beside it (previous/next, stop/skip) | `Button::large` | 44, 22 px glyph |
| Less-used controls (shuffle, repeat, the window's button) | `Button::secondary` | 36 (32 `compact`) |
| A widget's leading icon | set by Home (`Icon::large`) | `tileIconSize` 24 |

Spacing:
- A card's row of controls is `loose` (`looseItemSpacing` 24); groups of rows
  on a stacked card sit `apart` (`groupGap` 20). Spare height goes between
  groups, never as a band at the bottom (decision 107).
- Home's widgets: `tileHeight` 74, `tileGap` 12, `tilePadding` 10, so a
  CardTitle line and a Body line fit with room around them.
- Every card and Home share `expandedSize.y` (248) so switching tabs doesn't
  resize the island; a test pins media, the timer and Home to it.

## Where the dense scale stays

Lists and windows hold many rows to scan, not one thing to glance at; they
keep the smaller scale: `Title` (17/700), `Body`, `Caption`, `buttonSize` 36,
`compact` buttons in rows.
- Settings' list card and the windows' pages (decisions 030, 104).
- Search's results (`Result`, `ResultDetail`; decision 079).
- Grouped rows, menus, dialogs (decisions 020, 086, 090).
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
