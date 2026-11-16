# 119: The card scale a step smaller; media's bar between the cover and the controls

Status: Accepted (amends 107, 111, 114: their sizes; 107's bottom rows). Core tested; the look on Windows needs a run.

**Context.** After decisions 107 to 114 the user found the island's pages
(Home, media, the timer) too big, and asked for them a little smaller, with
a little more room between the pieces being fine. Media's progress bar sat
right over its controls, with the spare height between it and the cover;
it should be halfway between the cover and the controls.

**Decision.**
- **Sizes, a step down** (tokens): `heroFontSize` 56 (from 64), its line 52
  (60); `cardTitleFontSize` 20 (22); `expandedArtSize` 108 (120);
  `primaryButtonSize` 50 (56), compact 44 and its glyph 18 (48, 20);
  `largeButtonSize` 40 and its glyph 20 (44, 22); steps 30 and 14 long (34,
  16); Home's widgets 64 tall (68), padded 8 (9), their first line 17 (19),
  their icon 20 (22). `expandedSize` stays 248: the room left goes between
  the pieces.
- **A Standard card's bottom slot starts right under the art**, and its
  rows (`SlotFrame::atBottom`) still end at the card's padding, the spare
  height now spread evenly above each row instead of all above the first.
  Media's bar is halfway between the cover and its controls, and there's
  still no band at the bottom. Not for lists, which scroll in their windows.

**Consequences.** The test that media's rows leave at most 12 px spare is
now that the bar is halfway between the cover and the controls.
