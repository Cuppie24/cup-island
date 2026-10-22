# 071: To-do order, the Done row, toasts, quieter presses, a wider island

Status: Accepted

**Context.** A batch of requests from using the Pomodoro card: solved
to-dos crowded the list; an unticked one went to the end instead of where
it was; a deleted one was gone for good; scrolling while carrying a to-do
left it behind the pointer; statistics couldn't be started over; a press on
a card that stays open squashed the island for a click that does nothing;
and the island was narrow for its content.

**Decision.**
- **The Done row:** solved to-dos go under a "Done · N" row (chevron)
  after the open ones, in the same list, collapsed each run; clicking it
  shows them. A drag's index counts it among the list's rows.
- **Order:** each to-do has a rank, its place in the whole list. Open ones
  are kept in rank order, so one unticked returns where it was; a drag
  gives the moved one a rank between its neighbours. The file keeps its
  format and is written in rank order, so this survives a restart.
- **Toasts** (`Host::toast`, `Toast`): a short note with one action
  floating over the bottom of the provider's card, gone after
  `toastDuration`, when its action is clicked, or with its card; one at a
  time. It floats above the content (`Scene::toast`: its own pill and
  elements), which doesn't move for it: a row would have pushed the card's
  rows around. The pill is the theme's raised colour
  (`islandHighlightColor`). Like a toast from a toaster, it slides up from
  behind the card's bottom edge (clipped there) as it fades in, and back
  down past it as it fades out, over `toastSlideDuration`. Clicks on it never
  reach what's under it: its button takes them, the rest of it nothing.
  Pomodoro deletes a to-do at once and shows "To-do deleted · Undo"; Undo
  restores it in its place.
- **Reset statistics:** a quiet button at the bottom of the statistics
  page; it asks (destructive), then clears every day but today.
- **Carrying while scrolling:** a carried row moves with the list's scroll
  since it was picked up (`ListDrag::grabOffset`, the offset as shown), so
  it stays under the pointer while its drop place follows the rows going
  by; a list with a carried row scrolls at once (`InList::carrying`).
- **Presses** (`pressSquashes`, a host rule): a press on empty space
  squashes the island only where that click would close something; not on
  Home, a card that stays open (`Content::keepOpen`), or a pinned island.
- **Wider:** `expandedSize.x` 440 → 480 (every card and Home), the canvas
  540 wide, Settings' rows (`listCardWidth`) 440.
- **The primary button's glyph** is drawn unsnapped, centred on its ink,
  and a ▶ nudged right by `playGlyphNudge` of its width (a triangle's
  optical centre). `primaryHoverOpacity` and `playGlyphNudge` are shares,
  not lengths: display scaling leaves them alone.

- **Pixel snapping:** an element's visual took its offset rounded but its
  size rounded up, so at fractional frames (display scaling) it grew at
  the bottom and right: buttons, their hovers and glyphs sat up to a pixel
  low in their rows and in the toast. Edges now snap to the nearest pixel
  on both sides (`snapToPixels`), for elements, the marks behind them (a
  hovered row) and the toast's pill alike. An element in a scrolling list
  snaps where its visual is, in the list's scroller, not at its scrolled
  frame: that moved by fractions as the list scrolled, its snapped height
  flipped by a pixel, and the size change rebuilt (faded in) its rows, so
  checkboxes and buttons flickered. Only a change of its real size rebuilds
  an element.

**Consequences.** Scenarios in `home.scenario` (presses) and
`dialog.scenario` (toasts); `todo_tests.cpp` (order, the Done row, the
toast's Undo, carrying while scrolling); `pomodoro_stats_tests.cpp`
(reset). Drawing needs a Windows run.
