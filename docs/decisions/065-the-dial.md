# 065: The dial: the time in a ring on the card

Status: Accepted (amends 014)

**Context.** Decision 014 put a ring beside the card's countdown and
dropped it as clutter; rings stayed in the compact pill, the bubble and the
widget. A mockup of a calmer card put the ring *around* the time instead,
the phase name under the time, and nothing else for progress. That reads
at once and replaces the phase bar. Nothing could express it: items can't
sit on top of each other.

**Decision.**
- **`Dial`, one item:** a ring filling with its `Timeline` (the accent on
  the track, as a Progress ring, animated by the compositor), the time left
  big in its middle (Display style) and a short label under it. One item,
  drawn as a whole like Steps: not a ring holding other items, so the
  content model stays flat (decision 064 on tables, for the same reason).
- **Sized from its digits** (`dialDiameter`): the widest the time can be
  (its shape with the font's widest digit, as time labels are measured,
  014) and the label, as a block, with its corners on the circle, then
  `dialInset` more. A bigger font, the display's scale, `islandScale` or a
  live tokens.json edit keep the time inside the ring by construction.
- **Ticks like a time label:** layout fills its time at `now` and marks it
  ticking, so the scene is redrawn each second; its ring is re-armed only
  when its timeline changes.
- **Pomodoro:** the dial replaces the big time, the phase bar and the
  "Focus" line. Dormant: the planned focus in an empty ring, "Focus" under
  it. Running: the time left, the phase's name; the caption under the dots
  says only "3 of 8 done". The gear and the days in a row sit in the
  card's top corners, level with the ring's top (`Row::edgesAtTop`: a
  row's leading and trailing items at its top instead of centred), so they
  take no row of their own.
- **The time is bigger and lighter:** `displayFontSize` 34 → 48,
  `displayFontWeight` 450 → 300 (only Pomodoro uses Display). Windows 10's
  Segoe UI has no 300; it draws Light.
- **`canvasSize` 500×640 → 500×680:** the card with the dial and five
  to-dos needs 663; the canvas is the invisible layer the tallest card and
  its shadow must fit in.

**Consequences.** Layout and sizing are tested (`pomodoro_tests.cpp`:
the dial centred and square, sized from its digits, ticking; the corners
level with its top). Drawing it (the ring's shapes with the time on a
surface laid over them) needs a Windows run.
