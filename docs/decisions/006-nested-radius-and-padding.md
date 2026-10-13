# 006: One padding per mode, nested corner radii

Status: Accepted

**Context.** Content inside the pill looked slightly off: compact squares sat
further in horizontally than vertically, and art used a fixed 6 px radius
whatever the pill's corner was. Rounded shapes inside rounded shapes look
wrong unless their corners are concentric.

**Decision.** Each mode has one padding (`compactInset`, `alertPadding`,
`expandedPadding`), kept on every side of its content. Rounded content (art,
button hover shapes) gets the nested radius `pill radius - padding`, capped
at half its own size, where the pill radius is `pillCornerRadius()` (half
the height, capped by `maxCornerRadius`; the renderer draws the same curve).
`layout()` computes it into `Placed::cornerRadius`, so the renderer decides
nothing. `expandedSize` is tuned so the media rows end exactly one padding
above the bottom edge (a test pins it).

Click areas (`Placed::hitFrame`) take in half the gap to each neighbour and
half the row spacing, so there are no dead strips between controls. Rows
marked `loose` (media controls) use the wider `looseItemSpacing`.

**Consequences.** Changing `maxCornerRadius` or a padding in tokens.json
re-derives every inner radius. In Compact the nested radius makes the art
round (24 px square, 12 px radius). If squarer compact art is wanted, change
this rule (e.g. a minimum padding for radii), not a hard-coded radius.

**Alerts' leading square** is a square slot like the compact ends: its
content is centred in it, so an icon (an update, a low battery) sits in the
middle of the island's rounded left end, as far from the side as from the
top and bottom. It used to be laid out like a text slot, start-aligned, which
put the icon against the edge.
