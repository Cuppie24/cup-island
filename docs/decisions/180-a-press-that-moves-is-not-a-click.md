# 180: A press that moves isn't a click

Status: Accepted

**Context.** A control was clicked when the button was let go over it, however
far the pointer had travelled in between: across the button, or off it and
back. The user asked for the opposite, on every control: press, move, let go,
and nothing is pressed.

**Decision.**
- **Moved `dragStartDistance` from where it went down, a press is over.** What
  it was on springs back at that moment, not at the release, and letting go
  clicks nothing, back on the same control included. Less than that is a
  click as before: a hand isn't perfectly still.
- **Every control, both surfaces.** On the island: a card's buttons and rows,
  the page-row buttons, the top band, empty space (the click that opens or
  closes it), the bubble, a menu's items. In provider windows: everything
  `windowHitTest` gives a press to.
- **What a move is for goes on.** A scrub, a selection in a field, a list row
  or a window's card being picked up, the bubble's drag, the lift (decision
  026) all start with a press that moves; they're untouched. One distance
  says both "not a click" and "a drag", so there's no gap between them.
- **The keyboard's Enter and Space** click as they did.

**Consequences.**
- The app tracks where the button went down (`Pointer::down`/`moved`,
  `Shown::pressedAt`); core's `isDrag` is the one test of "moved".
- A slow press that drifts a few pixels does nothing. That's what was asked;
  the distance is a token if it proves too tight.
