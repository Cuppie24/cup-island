# 132: Dragging in windows glides, and a new group's name is focused

Status: Accepted (amends 129). Core tested; the feel on Windows needs a run.

**Context.** After the first run of dragging (decision 129) the user asked
for: one line as a group's handle, not the icon font's two bars, a little
under the card's edge and easier to hit; no task lit while over the handle
(the card's last one was); the other groups and tasks sliding as they make
room, not jumping; a soft outline where the carried group lands; and a new
group's name ready to type.

**Decision.**
- **The handle** keeps `Glyph::Grip`, but `paintGlyph` draws it as one
  rounded line (`windowDragLineWidth` × `windowDragLineThickness`), its box
  `windowDragHandleTop` under the card's top. Its click area is wider and
  taller than it draws (`windowDragHitWidth` × `windowDragHitHeight`, from
  the card's very top). Where that band reaches over the card's name, a press
  that never moves is the name's click, so renaming still works there.
- **Over the handle no row lights.** `windowRowAt` took the handle for a
  row named after its card ("card.x"), and the lit row was then every row
  whose key started with that: the last one won. The handle is no row now,
  and the lit row is only the hovered row's own items.
- **Gliding.** `follow` marks what's carried `Placed::carried` and the rest
  `Placed::glides`; just after a drop (or Esc), `settle` marks all of it, so
  the carried thing eases home and the provider's update lands softly, until
  the user does anything else (`Shown::settling`). A gliding element gets the
  list rows' implicit Offset ease (`ListViewports::setGliding`, decision
  029); one rebuilt elsewhere (a card's background grown or shrunk) is eased
  from its old place by an animation with both ends (decision 008). What's
  carried is raised over everything, its card's background too.
- **Where it lands:** `follow` puts a `GroupFill{ .outline }` (`kDropSpot`)
  under everything at the carried card's or task's laid-out place: a solid
  `trackColor` line, `windowDropOutlineWidth`, as round as what it outlines.
- **A new group's name gets the keys.** `WindowContent::focus` names a
  field's action and `focusRequest` changes each time it should be focused;
  the host focuses it once (`focusByAction`), its text selected, so typing
  replaces "Group 2".

**Not taken.** Animating a card's background's size as it grows or
shrinks: it's painted at its size, so it's redrawn at once and only slides.
