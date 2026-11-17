# 142: A group's "…" menu, and its name asked in a question

Status: Accepted (amends 117, 122, 134). Core tested; the look and feel on Windows need a run.

**Context.** A group's name was edited in place (decision 117): it lit under
the pointer like a button, and its delete showed at the row's end on hover.
The user didn't want the name to react to the pointer at all, and asked for
a "…" at the right of a group's header with Rename and Delete, Rename opening
a modal window.

**Decision.**
- **A window card's menu.** `WindowSection::menu` lists its items; the host
  puts a "…" (`Glyph::More`, `kWindowCardMenu` + the card's id) last in its
  title row. Clicked (or Enter on it), the island's menu (decision 086) opens
  under it, right edges together (`placeWindowMenu`), drawn by the island's
  `MenuLayer` over the window. An item clicked or Enter'd sends its action
  from the window; Esc, a click elsewhere or the "…" again closes it; the
  arrows, Home and End move its lit item. It closes when its card or its
  items change, under a question, and while something is dragged. Nothing
  under it lights while it's open.
- **A question with a field.** `Dialog::field` (windows only) puts a text
  field under the message, starting with that text. It takes the keys as the
  question appears, all its text selected (`dialogTakesKeys`); Enter confirms,
  Esc cancels, Tab and the arrows up and down go nowhere. Confirmed, by Enter
  or its button, the confirm action's `Action::text` is what's in it
  (`dialogFieldText`): a click on the button takes the keys from the field
  first, so what was typed is kept for it, and forgotten once answered.
- **Pomodoro.** A group's name is plain text. Its "…" holds Rename
  (`group.name.<id>`: "Rename group", its name in the field, confirmed as
  `group.rename.<id>`) and Delete (as before: asks first if it has tasks).
  A new group asks its name the same way at once ("Name the new group",
  "Group 2" selected): this replaces decision 134's focusing it in place.
  `WindowContent::focus` stays in the host, unused by Pomodoro now.

**Not taken.** A separate top-level window for renaming: the window question
(decision 122) is already a modal over the window, kept in it, with the keys.
