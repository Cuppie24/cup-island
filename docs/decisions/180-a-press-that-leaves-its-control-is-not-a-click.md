# 180: A press that leaves its control isn't a click

Status: Accepted

**Context.** A control stayed drawn pressed while the held pointer was
dragged off it, and was clicked if the pointer came back before the button
was let go. The user asked that a press dragged off a control is over, on
every control.

**Decision.**
- **Leaving the control ends the press.** The moment the held pointer is no
  longer on what it went down on, that springs back, and letting go clicks
  nothing, back on the same control included. Moving within the control
  changes nothing: it's a click wherever on it the button is let go.
- **Not re-armed on the way back.** Windows' and macOS's own buttons press
  again when the pointer returns, and click; Android's don't. The user's
  rule was "it left: not pressed", so it's the second.
- **Every control, both surfaces.** On the island: a card's buttons and rows,
  the page-row buttons, the top band, empty space (the click that opens or
  closes it), a menu's items. In provider windows: everything
  `windowHitTest` gives a press to.
- **What a move is for goes on.** A scrub, a selection in a field, a list row
  or a window's card being picked up, the bubble's drag, the lift (decision
  026) aren't clicks; they're untouched.
- **The keyboard's Enter and Space** click as they did.

**Consequences.**
- "Still on it" is the test the release already made (`onPressed` on the
  island, `samePress` in windows), now made on every move of a held pointer.
