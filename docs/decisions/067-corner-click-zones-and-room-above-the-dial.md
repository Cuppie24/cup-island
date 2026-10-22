# 067: Corner click zones, room above the dial, centred page titles

Status: Accepted (amends 066); the corners got their own row by 069

**Context.** The second run showed: the statistics and settings buttons
answered the pointer all down the ring's side; the ring touched the card's
top, level with the corner buttons; and page titles sat right of centre.
066 had also shrunk secondary buttons' hover circle, which made stop and
skip feel smaller: the complaint was the click zone, not the hover.

**Decision.**
- **Click zones:** a click area spans its row's height, so every button
  meets its neighbours with no dead strip. In a `Row::edgesAtTop` row the
  corner controls stop at their own bottom (plus half a gap): the dial's
  row is ~185 px tall, and a gear that lit up 150 px below itself was
  wrong. The middle item keeps the row.
- **Hover backgrounds are the full button again:** secondary buttons'
  hover is a circle their size (36), not 066's smaller one; the token is
  gone.
- **Room above the dial:** in an `edgesAtTop` row the middle items sit
  `groupGap` under the corners. The card grows by that, so `canvasSize`
  goes to 500×700 (the card with five to-dos still fits).
- **Page titles hug, centred:** a title beside a back arrow is a text that
  stretched over the room after the arrow and centred there, half an arrow
  right of the card's centre (as the big time did, 063). Titles are as wide
  as they are and centred on the card: Pomodoro's pages and Settings'.

**Consequences.** Tested in `pomodoro_tests.cpp` (the gear's zone, the
room above the ring, the title centred).
