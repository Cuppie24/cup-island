# 151: A toast stands apart

Status: Accepted (amends 071, 121). How the shadow is cast: superseded by 165.

**Context.** A toast ("Task deleted · Undo") is a pill in `islandHighlightColor`.
Over a window's page, itself a step above the island's colour, it was
barely there: two near-blacks side by side, no edge between them.

**Decision.** The toast is drawn like a row's menu, which already floats
over a card: the same faint edge (`trackColor`, `dividerThickness`), and a
shadow under it (`toastShadowBlur`, `toastShadowOffset`, `toastShadowColor`;
fainter on light themes, as their other shadows are). The pill sits in a
`LayerVisual` whose `DropShadow` takes the shape of what's drawn in it, so
the shadow has the pill's rounded corners. `LayerVisual.Shadow` is Windows
10 1607, so every supported Windows has it (decision 015); unlike the
island's shadow, it needs no `CreateVisualSurface` (1903). The fill stays
the theme's raised colour: every theme already has one.

**Consequences.** The island's cards get the same toast, edge and shadow
alike. The shadow is clipped with the toast at the card's or page's
bottom edge, which it slides up from.
