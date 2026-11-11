# 096: Glass on its own Settings page, with three transparency levels

Status: Accepted; built, not yet run on Windows (amends 089's Settings row)

**Context.** Glass (decision 089) had one strength, `glassOpacity` (or
`glassPlainOpacity` over the plain blur). How much of the desktop should
show through is taste, and depends on the wallpaper: people asked to pick
it, the way they pick how fast music colours fade (decision 053).

**Decision.**
- **Glass gets a page**, like Music colours: the list's Glass row says On or
  Off and opens it. On the page: the **Glass effect** switch (as before: on
  by default, decision 089) and **Transparency**, three levels picked in
  place (decision 093): **Low, Medium, High**. Medium by default, which is
  the strength glass had before, so nothing changes until someone picks.
- Each level has its own pair of opacities in tokens.json: how strong the
  island's colours are over the evened-out blur, and over the plain blur
  (which needs more):
  - Low: `glassDenseOpacity` 0.3, `glassDensePlainOpacity` 0.85;
  - Medium: `glassOpacity` 0.15, `glassPlainOpacity` 0.7 (unchanged);
  - High: `glassClearOpacity` 0.06, `glassClearPlainOpacity` 0.5.
- A pick applies at once and **fades** the fill (it's only its opacity:
  `onlyColorsDiffer`), over the picked colour-fade speed, like a theme.
- Saved as `transparency` ("low", "medium", "high"; nothing saved: medium).
  Windows 10 has no Glass row, so no choice either.

**How.** `Tokens::glassTransparency` is the pick, set by `app/live_tokens`
from Settings (not tokens.json) and kept over reloads, like `fadeSpeed`.
`islandFill` (core) reads it, so the pill, the bubble and the neck between
them all follow. "Transparency" is tagged New on the page, with a dot on
the Glass row (decision 048).

**Consequences.** Tested in `tests/settings_new_tests.cpp` (the page, the
three levels, saved and kept) and `tests/tokens_tests.cpp` (each level's
opacities, a pick rebuilds nothing). How each level looks needs a Windows
run; the six numbers are live tokens.
