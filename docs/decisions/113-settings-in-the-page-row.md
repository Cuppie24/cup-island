# 113: Settings is a button in the page row, right of the pin

Status: Accepted; amended by 165 (`Panel::rowSpot`: Search by Home's button, the theme switch by Settings') (amends 104: Settings' widget; 019: every panel a widget). Core tested; the look on Windows needs a run.

**Context.** Settings was a Home widget whose tap opened its window
(decision 104). It took a tile that shows nothing live, and it was only at
hand from Home. The user asked for it in the page row, right of the pin.

**Decision.**
- **`Panel::rowButton`**: a glyph. Set, the panel isn't a Home widget but
  a button in the page row's right corner; the pin moves left of it,
  `tabArrowGap` apart. A click sends the panel's `tapAction`, as its tile
  did; `Content::badge` puts a dot in its corner, like Home's button. It's
  grey until hovered, like the arrows, and a keyboard stop like the pin.
  More than one stack leftwards, the first in the corner.
- **Its id in the strip** is `kPanelButtonTab` + the panel's id, so it
  can't be mistaken for an activity's dot; the app routes its tap action
  like a tile's.
- **Home counts widgets, not panels** (`hasWidgets`): with only row buttons
  there's nothing on Home, so a click on the idle pill doesn't open it, as
  with no panels at all.
- **Settings uses it** (`Glyph::Settings`), with no tile content.

**Not taken.** A Settings-only button in core: the strip stays a host
part that providers fill, like Home's tiles (decision 014).

**Consequences.** Home has one widget fewer (the usual set: media and the
timer, then search and the theme), so it fits two rows more often.
