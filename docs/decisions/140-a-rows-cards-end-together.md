# 140: A row's cards end together

Status: Accepted (amends 117, 120; "New group" keeps its own height since 153). Core tested; the look on Windows needs a run.

**Context.** Cards sit in a grid row by row, each row as tall as its tallest
card (decision 120). A shorter card ended early and left a hole under it
before the next row. Asked whether a column layout (each card right under the
one above it) was wanted, the user chose to keep the rows and fill the hole.

**Decision.** `cardFrames` makes every card in a row as tall as the row's
tallest. Its rows still start at its top; the room is at its bottom. That
includes "New group" when it shares a row with a group: its dashed outline
is as tall as the group beside it. Dragging is unchanged: a card is a bigger
place to drop a task in.

**Not taken.** Columns (masonry): the user kept the rows.
