# 175: Shiny edges on either glass

Status: Accepted; built, not yet run on Windows (amends 165, 172)

**Context.** Liquid glass (decision 165) brought shiny edges: the rim's
line brighter, and a hairline round two corners of filled controls, the
skips, Home's widgets and a toast. Decision 172 already showed them on the
collapsed pill, which is the frosted glass under Liquid. None of them needs
liquid glass's captured picture: they're plain shapes. The user asked for
them on the frosted glass too.

**Decision.** The shiny edges go by the island being **glass**, not by
which glass: `liquidShine` and the rim's `glassRimAt` are on whenever
`Tokens::glass` is. A solid island (Glass off, a light theme, Windows'
transparency effects off) has none, as before.

**Consequences.** Frosted and Liquid now differ only in the picture behind
the island: Windows' blur, or the captured desktop bent at the lip. The
frosted glass's rim line is brighter than it was (`glassRimColor`'s own
opacity, 0.18, gives way to `glassLiquidHighlightOpacity`, 0.35): anyone
who liked the fainter line can lower that token. The tokens keep their
`glassLiquid…` names: renaming them would break hand-edited tokens.json
files for a name. Tested in `tests/tokens_tests.cpp`.
