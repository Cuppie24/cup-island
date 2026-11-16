# 123: The timer window's fourth run: hugging names, centred rows, caption glyphs

Status: Accepted (amends 117, 120 to 122). Core tested; Windows needs a run.

**Decision.**
- **A group's name hugs its text** (`Row::hug` with `edgesBeside` on a card's
  title row): its delete right after it, a gap apart, not at the card's
  edge; the row is lit, and pointed at, from its start to just past the
  delete (`slot_layout`: a hugging list item's lit part), and `windowRowAt`
  finds it only there. While the name is edited its field takes the row.
- **A row of cards short of the grid's columns is centred** (`cardFrames`):
  on a wide or maximized window, two groups sit in the middle.
- **A window question's corners nest its buttons'**: the panel's radius is
  its padding (`windowDialogPadding`, 20) plus the window's button radius
  (`windowNavRadius`), so Cancel and Delete are rounded, not square.
- **The title bar's buttons are Windows' own caption glyphs** (`Glyph::Minimize`,
  `Maximize`, `Restore`, `CloseWindow`: ChromeMinimize, -Maximize, -Restore,
  -Close) at Windows' size (`windowChromeGlyphSize`, 10).
- **"+ Add task" is a small button's height** (`smallButtonSize`, from
  `inputHeight`): a field with an icon reads as, and sizes as, a button.

**Addendum (decision 124's number, kept here).** The hugging name ended in
"…" ("T…"): an editable text is drawn `editInset` in from each side of its
frame, and its natural width didn't count that. It does now, for any
editable text laid out at its natural width. "+ Add task" is lower still:
`iconFieldHeight` (26), its own token.
