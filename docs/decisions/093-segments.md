# 093: Segments: a few options picked in place

Status: Accepted (Tab row's page from decision 048 and Colour fade's choices from 053 replaced)

**Context.** Picking one of two or three options meant a page of its own:
Tab row opened a page with Top and Bottom, Colour fade was three ticked
rows under a label. A page for two options is a click in, a click to pick
and a click back, and hides what's picked behind a chevron.

**Decision.**
- **`Segments`**, a content item: the options side by side in a pill-shaped
  track (`segmentHeight`), the picked one on a filled pill
  (`segmentInset` in). Every option is as wide as the widest label plus
  `segmentPadding` a side, so the pill doesn't change size as it moves. A
  click on option i sends `actionId` + i, like the choice pages' actions
  did; the provider moves `picked` and republishes.
- **Geometry is core's** (`core/segments.h`): the width, each option's
  frame, the option at a point. Layout, hit-testing, the keys and the
  renderer use the same functions, so a click lands on what's drawn.
- **Keys**: it is one focus stop; Enter or Space picks the next option,
  round to the first. Its focus ring is a pill like it.
- **Look**: the track is `islandColor` (the island showing through the
  group: a recess), the picked pill `buttonColor` with its label in
  `islandColor` (like a primary button, decision 064), the others'
  labels `bodyTextColor`. No new colours, so every theme has it.
- **Settings**: Tab row is a row on the list ("Home and pin buttons" under
  it) with Top | Bottom; its page and `kOpenTabRow` are gone. Music
  colours' page has Colours from music and Colour fade (Fast | Medium |
  Slow) in one group. A row with Segments isn't one button: only the
  options are clicked, so it doesn't light up as a whole.

**Consequences.** Geometry in `tests/group_tests.cpp`, Tab row and Colour
fade (clicks, Enter) in `tests/settings_new_tests.cpp` and
`tests/keyboard_tests.cpp`. The "New" tests now use Music colours' page
as the page with a new item on it. The look needs a Windows run.
