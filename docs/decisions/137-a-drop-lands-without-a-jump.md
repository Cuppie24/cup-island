# 137: A drop lands without a jump back

Status: Accepted (amends 129). Core tested; the feel on Windows needs a run.

**Context.** Let go of a dragged group, the others were already where they'd
stay, but for a moment everything went back to where it was before the drag
and then slid forward again. The app laid the page out without the drag
first (the provider's content still in the old order), and only then sent the
move; the provider's update relaid it in the new order, and decision 132's
glide played both changes.

**Decision.**
- **The move first, then the layout.** On a drop the app routes the move
  action, then lays the page out. A provider's update reaches the registry at
  once (Host::updateWindow), so that layout is already the moved page: from
  the preview straight to it, nothing moves but the carried thing, gliding
  into its place.
- **A task keeps its id when moved within its group** (`TodoList::place`):
  its rows' keys stay the same, so it glides into its place instead of
  fading out at the pointer and in at its place. Into another group, it
  keeps its id too if that group has none like it, but its card's name is in
  its key, so it still fades there.
