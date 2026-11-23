# 114: Home's widgets a little smaller than the cards

Status: Superseded by 170 (no widgets). Was: Accepted (amends 111: a widget's sizes). Core tested; the look on Windows needs a run.

**Context.** Decision 111 put Home's widgets on the cards' scale: the
22 px CardTitle at 800, 74 px tiles 12 apart, a 24 px icon. Beside each
other they read too big; the user asked for them a little smaller.

**Decision.**
- **`TextStyle::TileTitle`**: a widget's first line, 19 px at 700
  (`tileTitleFontSize`, `tileTitleFontWeight`): between the island's Title
  (17/700) and a card's CardTitle (22/800). Providers write it in `tile`;
  their cards keep CardTitle. The second line stays Body.
- **Tiles**: `tileHeight` 68 (from 74), `tileGap` 10 (from 12),
  `tilePadding` 9 (from 10): a 25 px TileTitle line, a 20 px Body line and
  the row spacing fit the 50 px inside. `tileIconSize` 22 (from 24).

**Consequences.** Two rows fit the shared 248 card with more to spare;
three need 260 (from 282), so Home still grows a little at three.
