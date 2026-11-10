# 094: The media card: one hero control, a bigger cover, steady times

Status: Accepted (amends 064's "media keeps its controls as they are")

**Context.** On the expanded media card every control had the same weight:
previous, play, next, shuffle and repeat were all 36 px glyphs in the same
colour, so nothing stood out. The cover (68 px, as tall as the three lines
of text beside it) looked small in the card. Shuffle's and repeat's "on"
was a 2 px dot under the glyph, easy to miss. The times beside the bar
shifted sideways as their digits changed, and sat a little off the bar's
line.

**Decision.**
- **Three weights of control.** ▶/⏸ is `Button::primary` and `compact`: a
  filled circle `compactPrimaryButtonSize` (48) wide, glyph
  `compactPrimaryGlyphSize`. Pomodoro's primary stays 56: it is alone in its
  row, the media one sits between two skips. Previous and next stay bright
  36 px buttons. Shuffle and repeat are `Button::secondary`: grey until
  pointed at or on.
- **A toggle that's on sits on a soft circle** in `tabSelectedColor`, its
  glyph at full strength. That is the "picked" fill already used for the
  shown tab, so every theme and every cover tint (decision 051) has it
  without a new colour. The dot is gone. The cover's accent is not used on
  the glyph: on an island tinted by the same cover it can disappear, and
  accents are for live data only (decision 010).
- **A bigger cover**: `expandedArtSize` 68 → 88 (+29 %), with a soft shadow
  (`artShadowBlur`, `artShadowOffset`, `artShadowColor`; lighter in the light
  themes). Core marks it (`Placed::raised`, Expanded's leading square only),
  so the small covers in the pill and alerts get none. The shadow is a
  DropShadow masked to a drawn rounded rect (Windows 10 1607+), not to the
  picture: a cover read later (decision 056) fades in without the shadow
  changing.
- **The card grows to fit**: a Standard card is now as tall as its art plus
  its bottom rows (`standardCardHeight`), never less than `expandedSize.y`.
  The media card goes 171 → 203; cards with little under their art keep
  171. A floating Standard card uses the same rule.
- **Steady times**: Digits and Display text (time labels, the big timer,
  the Dial) is drawn with tabular figures (OpenType `tnum`), and measured
  with them. A Digits line is centred on its digits (half the cap height
  above the baseline), not on its line box, so `0:42` sits on the progress
  bar's line (`platform/figures`).

**Not taken.** The cover blurred into the card's background: the island
already takes on the cover's colours (decisions 051, 054), a blur over that
repeats it, costs a live blur on an always-on window and makes text harder
to read. A like button: Windows' media sessions have no such command. Mouse
wheel volume: left for later (media sessions don't carry volume, so it means
the system's or the app's own mixer).

**Consequences.** The media card is taller than the other cards, so
switching tabs between it and a 171 px card morphs the height (as stacked
cards already do). The platform half (shadow, toggle fill, figures) is only
verified on Windows.
