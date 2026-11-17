# 132: "Add task" opens a row under it

Status: Accepted (amends 125 and 127: the field no longer turns into the row). Core tested; the glide is verified only on Windows.

**Context.** Clicked, "+ Add task" turned into the task row being written
(decision 125): the button was gone while writing, and a second task meant
finding it again under the one just added.

**Decision.**
- "+ Add task" is a button (`task.write.<group>`), the whole row its click,
  its + where a task's box is. It stays.
- Clicked, the provider opens a row being written right under it
  (`TextInput::autoFocus`); the window gives it the keys as it appears, if
  nothing else has them (`focusArrived`).
- Enter adds the task and keeps the row and its keys, for the next one.
- Left (a click elsewhere, Tab, another field), the row sends its
  `TextInput::leaveAction` (`task.written.<group>`) with what's in it: added
  if it isn't blank, and the row goes either way. Esc sends it blank: dropped.
  This replaces 127's "a focusedIcon field sends on leave": the provider has
  to hear about a blank leave too, to take the row away.
- Editing an existing task is unchanged: emptied, it keeps its old text.
- Appearing: the row fades and grows in as new content does (decision 080),
  and what's under it on the page glides down (`ElementVisuals::apply`'s
  `glide`) instead of jumping. Only when the window kept its size and page,
  never for what a drag carries (`Placed::carried`), which follows the pointer.

**Consequences.** A window page's moved items glide on any content change
(a task wrapping onto a second line, a drop), as a list's rows do. A resize
still lays them out at once.
