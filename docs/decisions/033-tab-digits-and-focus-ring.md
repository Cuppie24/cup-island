# 033: Tab, Ctrl+Tab, the digits, Esc one step back, a focus ring

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
  the first after the last; **Shift+Tab** to the one before
  (`previousFocus`). **Home** and **End** go to the first and last.
- **Ctrl+Tab switches tabs**: Home, then each activity in strip order,
  round again (`nextTab`); **Ctrl+Shift+Tab** the other way
  (`previousTab`). On a dormant panel's card (no tab shown) both go to
  Home. As in a browser, Ctrl+Tab is tabs and Tab is items.
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
- **Back on Home, focus returns to the widget the keys last clicked**, not
  the first one, so a row of widgets can be looked through one by one.
  Wherever else what had focus goes away (a page opened, a tab switched),
  focus moves to the first item, as 031 did after Enter.
- **Esc steps back one level** instead of always closing (031): a card's
  own page goes back to its list (`Content::backAction`, sent like a click
  on its back arrow: Settings' pages); a card opened from a Home widget
  goes back to Home (`IslandState::cardFromHome`); anything else closes. A
  card reached another way (the pill, a tab, an alert) has no Home to go
  back to, and switching tabs forgets it came from Home.
- **Up and down walk a scrolling list to its end** before leaving it. Rows
  out of a list's window sit where they would be scrolled in, which can be
  under the tab strip; by position alone, down from the last theme in view
  went to the Home tab.
- **At a list's last stop, up and down scroll it** a `scrollStep` while
  it can still scroll that way, focus staying put; then the next press
  leaves it. Rows with nothing to click (Settings' list of keys, which now
  scrolls: it had grown to the canvas's height) come into view this way.
  A focused row scrolled out of its window has no ring.
- **Dialogs take the keys** (decision 020 had only Esc): the arrows, Tab,
  Home/End and Enter/Space move between Cancel and the answer and press
  one. Focus stays in the dialog, as in any dialog: the tabs and Home
  behind it aren't stops, and Ctrl and Alt keys do nothing while it's up.
  The first press shows focus on Cancel, the first button, so a stray
  Enter answers nothing; Pomodoro's Ctrl+W (decision 034) is answered
  without the pointer.
- **The demo keys (M, T, C, D, H) are Debug builds only.** They fake states
  for development; in Release a stray key on the island did odd things.

**Consequences.** The pure parts are tested in `tests/keyboard_tests.cpp`
(static stops, Tab and Shift+Tab round, Home/End, digits, Ctrl+Tab both ways, walking the theme
list, scrolling the list of keys, dialogs, Settings' back action); Esc back to Home in `home.scenario`.
Settings' "On the island" list gains the new keys. The ring, returning to
the widget and the key handling need a Windows run.
