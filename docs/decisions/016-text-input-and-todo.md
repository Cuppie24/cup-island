# 016: Typing into the island, and Pomodoro's to-do list

Status: Accepted

**Context.** Pomodoro should keep a simple to-do list under its timer, with
"Clear solved" and "Clear all". Items are added by typing, or by pasting,
where every line becomes a task. The island had no text input: providers
can't draw (PS §5.3), and nothing typed reached the island.

**Decision.**
- **`TextInput` item:** a one-line field the host draws and edits. A click
  focuses it; the window already takes focus when clicked, so typing
  arrives as `WM_CHAR`. The text being typed is core state
  (`IslandState::draft`): one draft at a time, on the card that's shown.
  Core rules, tested: focus only on the shown card; typing, Backspace
  (a whole UTF-8 character), Enter (the app sends `Action{actionId, text}`,
  the field empties and keeps focus), Esc (stop typing; a second Esc closes
  the card). Switching or closing the card drops the draft. While the field
  holds unsent text, Expanded doesn't time out, so it never eats what you
  typed. While a field is focused, keys go to it, not to the demo hotkeys.
- **Paste:** Ctrl+V in the field sends the clipboard's text as the field's
  action, and the paste button asks the provider to read it (handed in, so
  the provider stays pure). Pomodoro makes every non-blank line a to-do.
  Dropping text on the island waits for a real drop target (the D key only
  simulates a drag today).
- **Caret:** a still bar after the text, no blinking: nothing runs between
  keys (TR §8). A text longer than the field shows its end.
- **Buttons grow two options:** `label` (a text button, as wide as its
  label: "Clear all") and `compact` (`smallButtonSize`, for list rows: a
  to-do's checkbox, the paste button). `Text::struck` strikes a solved one.
- **Centred cards grow to fit** their rows, from Expanded's size up to the
  canvas (now 420×400). The provider caps its list (`todoVisibleItems`,
  then "+n more"), so the tallest card still fits.
- **Pomodoro's list** (`providers/pomodoro/todo_list`): pure, tested. Under
  the timer on the running card, and on the dormant card too (now a
  centred card: ▶, the full focus time, the plan), so the list can be
  made before starting. Saved on every change to
  `%LOCALAPPDATA%\CupIsland\todos.txt`, one per line ("[x] " / "[ ] ").

**Consequences.** Rules in core (`island.cpp`, `slot_layout.cpp`), tested by
`tests/scenarios/input.scenario` and `tests/todo_tests.cpp`. The platform
side (WM_CHAR, the clipboard, drawing the field) needs a Windows run.

**Open.** No cursor movement or selection inside the field (type and
Backspace only). No editing or reordering a to-do, no deleting one alone
(solve it, then Clear solved). Drag-and-drop of text.
