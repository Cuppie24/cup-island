# 105: Text fields in providers' windows

Status: Accepted. Core tested; the Windows side compiles in CI, needs a run.

**Context.** Windows (decision 104) drew a provider's `TextInput` and its
texts that edit in place (`Text::editAction`), but nothing could type in
them: a click on a field sent its action with no text, and Enter on it did
the same. A provider whose window holds a list people add to (Pomodoro's
to-dos, its full view) needs them. The island's typing lives in
`IslandState::draft` and `reduce()`, which are the island's, not a window's
(decision 104 keeps the two apart).

**Decision.** A window types the way a card does, with its own state.
- **`WindowTyping`** (`core/window_fields.h`), one per window, kept by the
  app beside its scroll and keyboard focus: the field with the keys (a
  `Draft` whose `activityId` is the window's id) and the text left in the
  others (`kept`, decision 050). `windowLayout` takes it and fills the page's
  fields from it, as `layout()` fills a card's, so the island's painters draw
  the caret and selection unchanged.
- **The rules are core's** and tested: `focusField` (a click, with the caret
  where it went; the keyboard landing on a field), `fieldKey` (Enter, Esc,
  Backspace, Delete, the arrows, Home, End, Ctrl+A, with Ctrl by word and
  Shift selecting: text_edit's, decision 043), `typeText` (typed or pasted:
  one line in at the caret, past `maxLength` what fits, decision 045; several
  lines pasted into a field sent whole, as on a card), `blurField`,
  `dropGoneField`.
- **What's sent** is a card's Action with `windowId` set: Enter sends the
  field's text and empties it, the field keeps the keys; a live field
  (`changeAction`, decision 079) reports every edit; a text edited in place
  sends its new text on Enter or when the field lets go (a click elsewhere,
  Tab), and Esc drops it.
- **Keys.** While a field has the keys it takes a text box's; Tab, Up and
  Down, Page Up/Down, Ctrl+Tab and Ctrl+W let go of it and then do what they
  do in a window. Esc stops typing; a second Esc steps back or closes, as
  before. The keyboard landing on a field (Tab, the arrows, Enter) gives it
  the keys, as Tab does in any Windows dialog.
- **On Windows** (`app/window_typing`): `WM_CHAR` (UTF-16 to UTF-8,
  surrogate pairs) types; keys come from `WM_KEYDOWN`; Ctrl+C/X/V use the
  island's clipboard helpers with the window as owner; the caret goes where
  a click lands (`ProviderWindow::caretAt`, the same DirectWrite layout the
  painter uses); double and triple clicks select a word and everything; a
  drag selects.

**Not taken.** Reusing `IslandState::draft` for windows: one draft for the
island and every window would steal the island's typing whenever a window
was clicked, and puts windows back into the state machine. Row menus and
dragging list rows in windows stay for later; a window's lists don't scroll
on their own (decision 104).

**Consequences.** A window's kept text goes when the window closes (a card's
stays until the app quits): reopening a window starts clean. The pointer
stays an arrow over a field; an I-beam needs `WM_SETCURSOR` hit-testing,
left for later.
