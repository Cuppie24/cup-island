# 050: A field's unsent text survives

Status: Accepted (amends 016)

**Context.** Text typed into a field (Pomodoro's "Add a to-do…") was the
focused draft's and nothing else's: the card closing (the idle timeout, a
click outside), a tab switch, a dialog or Esc dropped it. Half a to-do,
gone because the island collapsed.

**Decision.**
- **Kept, by card and field** (`IslandState::keptText`, keyed
  "<activityId>/<actionId>"): whenever a field's draft goes (`dropDraft`:
  the card no longer shown, another field focused, a dialog, Esc), its text
  is set aside if there is any.
- **Shown and resumed:** the unfocused field shows it, in place of its
  placeholder; focused again (a click, Enter on it) the draft starts from
  it, the caret where it was clicked, else at its end.
- **Only Enter clears it** (sent): the field empties as before.
- **Not for edits in place** (a to-do being renamed): leaving one already
  saves or drops it (decision 043), and the next edit starts from the
  to-do's own text.
- In memory only: a restart starts with empty fields.

**Consequences.** `card_overlays.cpp` (`dropDraft`, `focusInput`),
`slot_layout.cpp` fills the unfocused field. Tested in
`tests/scenarios/input.scenario` and `tests/todo_tests.cpp`. Unsent text
still keeps the card open only while the field is focused (016).
