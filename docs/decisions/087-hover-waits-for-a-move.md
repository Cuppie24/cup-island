# 087: Hover waits for the pointer to move

Status: Accepted; built, not yet run on Windows

**Context.** The owner, on search: the result under the mouse was lit even
though the mouse hadn't moved, so it looked like the row Enter would open.
Enter opens the first row (decision 079), so the two lit rows contradicted
each other. It isn't only search's: any card that opens, or changes, under
a still pointer lit whatever came under it (a Settings row, a Home tile, a
row scrolled in by the keys).

**Decision.** For the whole island, not one card: the pointer lights
something (hover, a list row's background, revealed buttons, a menu item)
only once it has moved since what's under it last changed some other way.
- **Still again** when a card opens or switches under it (any surface,
  decision 081), and on every key press (typing changes search's results;
  the keys lead anyway, decision 031).
- **Moving** is a real move (another point: Windows repeats a move at the
  same point when windows change under a still pointer), a press, or the
  wheel (scrolling is the pointer at work, so what scrolls under it lights).
- **The cursor isn't held back**: the I-beam over a field follows the
  pointer, still or not. Only what lights up waits.
- A row's menu (decision 086) follows the same rule: one opened by the keys
  under a still pointer keeps its first item lit.

It lives where hover does, in the app's pointer code shared by every
surface (`Pointer::still`, `hoverPoint` in `app/main.cpp`); core has no
hover. Launchers (Spotlight, PowerToys Run) do the same.

**Consequences.** After opening a card with a click, the button that was
under the pointer isn't lit until the pointer moves a pixel; a click still
works at once, since clicks hit-test where they are, not what's lit.
