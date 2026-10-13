# 043: Editing text like a text box; a to-do's checkbox is its own target

Status: Accepted (supersedes 016's "type and Backspace only" and its `Row::clickAnywhere` for to-dos)

**Context.** A to-do's whole row ticked it (016), so a click meant to edit
or select its text ticked it instead, and the checkbox itself was a small
target. Editing (029) was type-and-Backspace at the end of the text: no
arrows, no selection, no clicking into the middle. Native text boxes
(EDIT/RichEdit, XAML) can't be used: the island is drawn through
Composition (decision 003), a Win32 child control would neither follow the
morph nor match the island's fonts, colours and clipping.

**Decision.**
- **The editing is ours, in core** (`core/text_edit`): `Draft` has a
  `caret` and an `anchor` (UTF-8 byte offsets, always on a character's
  first byte). Events: `TextTyped` replaces the selection, `TextErased`
  (Backspace/Delete, Ctrl for a word), `CaretMoved` (arrows, Ctrl+arrows,
  Home/End; Up/Down go to the start/end of the one line, or a line up or down in wrapped text, decision 045; Shift selects),
  `CaretPlaced` (a click, Shift+click and drags select), `WordSelected`
  (double-click), `AllSelected` (Ctrl+A, triple-click). Word jumps work
  like Windows': letters and digits (anything outside ASCII counts as a
  letter) or a run of punctuation, then the spaces after it.
- **Clipboard:** Ctrl+C / Ctrl+X copy and cut the selection. Ctrl+V of one
  line goes in at the caret; several lines in the add field still become a
  to-do each (016); edited in place, the first line goes in.
- **Pixels** (`platform/field_painting`): the selection in `selectionColor`,
  a still caret (no blinking, TR §8), and text longer than the field
  scrolled just enough to show the caret. The scroll is worked out from the
  caret alone, not remembered, so painting and click hit-testing (DirectWrite
  `HitTestPoint`) agree without shared state. DirectWrite counts UTF-16; core
  converts (`utf16Index`, `utf8Offset`).
- **A to-do's text is clicked into, not ticked:** `hitTest` returns an
  editable `Text`'s `editAction`; a click there edits it in place with the
  caret where it was clicked (Enter or Space on it: at the end). A press
  dragged still picks the row up (029). The I-beam shows over a field only: a to-do's text gets it once it's being edited.
  Editable text is drawn `editInset` in from each side of its stretch; the
  field drawn in its place (`TextInput::inPlace`) fills the whole stretch,
  as tall as the checkbox, so its background keeps the row's usual gap to
  the checkbox and the delete button, and the text keeps its font and
  position. Text and field swap without a fade, so nothing flashes.
- **No edit button:** a to-do's text stretches from its checkbox to its
  delete button, so a click anywhere on the task but those two edits it
  (past the end of the text: the caret at the end). The hover-revealed edit
  button (029) is gone; `Button::editsText` stays in the content model.
- **Moving on keeps the edit**, like any text box: a press anywhere but the
  field, or the island losing focus, sends the edited text. Only Esc drops it.
- **The checkbox is the target:** a list item's first and last items' click
  areas reach the row's edges (top to bottom, start to the text), and it
  has its own hover shape again. `Row::clickAnywhere` stays for Settings'
  rows, where the whole row is one action.
- **Bigger:** `smallButtonSize` 24 → 32, `listItemPadding` 4 → 6 (= `listItemInset`, so rounded content in a row nests in the row's corners: radius = row radius − inset, decision 006),
  `inputHeight` 30 → 36, Centred cards `centredCardWidth` 440 wide (from
  380), and the canvas 500×560 to hold them.

**Consequences.** Rules tested by `tests/scenarios/text_edit.scenario`,
`tests/todo_tests.cpp` and the offset conversions in `unit_tests.cpp`. The
drawing, the hit-testing and the keys need a Windows run. No undo (Ctrl+Z),
no IME composition window placement, no right-click menu.
