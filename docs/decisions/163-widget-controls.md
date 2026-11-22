# 163: A widget can have controls, in a column at its right

Status: Accepted (amends 019: widgets only inform; 009: Home keeps the cards' width). Core tested; the look on Windows needs a run.

**Context.** Decision 019 took every button off Home's widgets: small buttons
on small tiles were easy to mis-hit, and a tile meant different things
depending on where it was clicked. The home redesign asked for the most-used
controls back on the widgets (play and pause without opening the card), in
a way that keeps the face of a widget one target.

**Decision.**
- **`Content::tileControls`**: up to three items (a `Button` or a `Switch`)
  in places `before`, `primary`, `after`. The provider decides whether a
  widget has any; leaving it empty keeps the widget information only, as
  in 019. Buttons and fields in `tile` are still left out.
- **One column, one axis.** Home places them in a column of fixed width at
  the widget's right, inside its padding: `smallButtonSize`,
  `largeButtonSize`, `smallButtonSize` with `tileControlGap` between
  (108). Each sits centred in its place, so the primary is on the same
  line on every widget, and a lone control (a switch) still takes the
  whole column and sits in its middle. Sides are compact buttons, the
  middle a large one.
- **Only with room.** The rows beside keep at least `tileTextMinWidth`;
  narrower than that, the widget shows no controls (it only informs).
- **The face is the rest.** `Tile::controls` is the column; `Tile::face()`
  the rest, what lights up and opens the card. A click in the column off
  a control does nothing, so a near miss never opens the card.
- **Keys.** Controls are ordinary buttons, so they're stops: Tab goes to a
  widget, then its controls left to right, then the next widget. Enter on
  the widget clicks its face.
- **Home is wider:** `homeCardWidth` (680), so two wide widgets side by
  side (media and the timer) each keep `tileTextMinWidth` beside their
  controls. Cards keep `expandedSize`; whether they follow is open.
- **A question from a control opens the card.** The app sends
  `WidgetActed` just before a control's action; a question its provider
  asks right then (Pomodoro's stop, decision 020) opens the widget's card
  with the question on it, as a click on the widget would. A question
  asked at any other time on Home still opens nothing.
- **Media:** back, play/pause, next. **Timer:** stop, pause/resume, skip;
  dormant, start alone in the middle. Shuffle, repeat and seeking stay on
  the card (a seek bar in the face would be a target inside a target).

**Consequences.** Rules in `core/home.cpp` (`placeControls`), tested in
`tests/widget_controls_tests.cpp`; the question in
`tests/scenarios/dialog.scenario`; the widgets' controls in
`tests/media_tests.cpp` and `tests/pomodoro_tests.cpp`.
