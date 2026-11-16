# 120: The timer window's alignment, spacing and hover fixed

Status: Accepted (amends 117: cards share the row's width; a card is one background). Core tested; the look on Windows needs a run.

**Context.** The first run of decision 117's window showed:
- the header's time sat a few pixels under its buttons;
- the phase and round hugged the steps;
- the task groups were small (340 wide, with a gap at the row's right) and
  their names cut off;
- task rows far apart, no room at a card's left;
- the in-place field small next to the text it replaced;
- no delete on hover anywhere in a window;
- the window's title flickering while it was resized.

**Decision.**
- **The Hero time is centred on its digits**, as a `Digits` time already
  was (`figureNudge`, decision 094), in both its painters (whole and rolling).
  Centred by its line box, Segoe UI's digits sat ~3 px under the box's middle.
- **The phase and round take the header row's spare width, set against
  its end**, so the loose row's gap (`looseItemSpacing`) separates them from
  the steps. Edge items stay packed (media's shuffle and repeat).
- **Cards share the row** (`cardGrid`): as many columns as fit at
  `windowCardMinWidth` (440) or more, the row's width split between them.
  This reverses 117's one width: the cards were too small, and with the
  next fix a resize no longer flickers, so stretching costs only a repaint.
- **A card is one background** (`<card>/fill`, a GroupFill) with its rows
  `windowCardPadding` inside, `Row::inset`, not a group of padded rows (11
  above and below each). Tasks drop the list item's padding: 36 px apart
  (from 58). The name sits in line with the checkboxes under it.
- **The title row's name takes the row**: its count and delete go at its
  end (`titleItems` then `titleTrailing`), so a long name isn't cut and its
  field has the row's width.
- **An in-place field has its text's style** (`TextInput::style`): a
  window's text is `WindowBody` (decision 112), and the field drew at the
  island's 15 px and line height.
- **Hover rows in windows**: `windowRowAt` (core) gives the page's row under
  the pointer, within the card under it; the window reveals that row's
  `revealOnHover` buttons, as the island does with `rowAt`. Windows never
  set it, so a task's delete never showed.
- **No fade on a resize**: an item rebuilt only because its size changed is
  rebuilt in place (`ElementVisuals::update`), as a resnapped one was. Each
  frame of a window's resize faded the title and any stretched text in
  anew. The title bar's title is as wide as its text, so it isn't repainted at all.
