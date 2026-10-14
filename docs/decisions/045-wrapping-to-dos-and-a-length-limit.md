# 045: Long to-dos wrap; a to-do has a length limit

Status: Accepted

**Context.** A to-do was one line ending in "…", so a long one couldn't be
read without editing it, and nothing stopped a to-do from being an essay
(200 bytes, cut wherever byte 200 fell, even inside a character).

**Decision.**
- **`Text::wrap`:** the text takes as many lines as it needs. Core breaks
  the lines (`core/text_wrap`): word by word at the width the text gets,
  measured with the real font (`MeasureText`), each word rounded up, so a
  line is never wider than measured. Only at spaces: a word longer than a
  line keeps its own line and ends in "…".
- **Core decides, the painter follows:** the Placed Text carries
  `wrapped`, its value with the breaking spaces turned into '\n'. Same
  length, so caret and selection offsets hold. The painter draws those lines
  exactly `lineHeight` apart (uniform DirectWrite line spacing), so what's
  drawn is as tall as what was laid out; it never rewraps.
- **Rows grow:** a slot's first layout pass finds each wrapping text's
  width, and its row grows to its lines; the checkbox stays centred on it.
  A list's window still counts rows of one line (`planRows`'
  `windowHeights`), so one long to-do doesn't stretch the window. A
  stacked card's height counts wrapped rows (`slotHeight` with the measure
  and the draft).
- **Dragging a taller row** (decision 029): it goes past a row above
  once its top reaches that row's top, and past one below once its bottom
  reaches that row's bottom. For rows of one height that's where their
  centres cross, as before; by centres alone, a wrapped to-do could never
  pass a shorter one at either end of the list, held back by the list's edge.
- **Editing wraps the same way:** the in-place field gets `wrapped` from
  the draft, grows a line at a time as it's typed into, and Up/Down move
  the caret a line (from the first or last line: to the start or end).
- **`todoMaxLength`** (150 characters, tokens.json): the add field and an
  edited to-do stop taking characters there (`Draft::maxLength`:
  typing and pasting put in only what fits); the list caps what's added,
  renamed or loaded the same way. Counted in characters, cut only between
  characters.

**Consequences.** `tests/todo_tests.cpp` (wrapping, rows, the window, the
growing field, the cap) and `tests/scenarios/text_edit.scenario` (the
limit). Wrapping costs a measurement per word on layout; the renderer's
measure cache holds 2048 strings so a card's words stay cached. Drawing
and Up/Down need a Windows run.
