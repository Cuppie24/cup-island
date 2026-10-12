# 033: Tab, Home/End, the digits, Shift+Tab for tabs, and a focus ring

Status: Accepted (extends 031)

**Context.** Decision 031 left two things open: Tab didn't move focus, and
focus looked exactly like hover, so with the pointer resting on one item
and focus on another there was no telling which one Enter would click.
Home's static widgets (Bluetooth: no card, no tap action) weren't stops
either, so down from a row of cards over a row of static ones jumped a
whole row.

**Decision.**
- **Every Home widget is a stop**, static or not. Enter on a static one
  does nothing, like a click. It still gets no hover fill (it doesn't
  react to the pointer); the ring shows it has focus.
- **Tab** goes to the next item in reading order (`nextFocus`), round to
  the first after the last. **Home** and **End** go to the first and last.
- **Shift+Tab switches tabs**, not focus backwards: Home, then each
  activity in strip order, round again (`nextTab`). On a dormant panel's
  card (no tab shown) it goes to Home. It's what people reach for most on
  the card, and the arrows already go backwards.
- **1 to 9 on Home** click its widgets in publish order (`homeWidget`):
  focus lands on the widget, then it's clicked as if Enter were pressed,
  so a static one just shows focus. Top row or number pad, without Ctrl;
  only while Home is shown, so the digits mean nothing elsewhere.
- Every key starts where 031's arrows do: with nothing focused, the first
  press only shows focus on the first item. Opening Home with the shortcut
  still focuses nothing, by choice: that first press is the one that shows it.
- **A focus ring**, drawn only for the keyboard's focus: a stroke
  `focusRingWidth` wide in `focusRingColor`, inside the item's frame with
  its hover shape's corners (`Focusable::radius`), over the content. Each
  theme sets the colour to its accent; Midnight's is translucent white.
  The pointer taking the lead back hides it. It moves at once and only
  fades, like hover: there is no glide to hand off (decision 008).
- Focus is dropped when the card closes, so it never lights the pill's own
  buttons in Compact and the next card starts afresh.

**Consequences.** The pure parts are tested in `tests/keyboard_tests.cpp`
(static stops, Tab round, Home/End, digits, Shift+Tab). Settings' "On the
island" list gains the new keys. The ring and the key handling need a
Windows run. No state rule changed: `reduce` sees the same events a click
sends.
