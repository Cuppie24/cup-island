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
  place (decision 093): **Low, Medium, High**. Medium by default.
- Each level has its own pair of opacities in tokens.json: how strong the
  island's colours are over the evened-out blur, and over the plain blur
  (which needs more):
  - Low: `glassDenseOpacity` 0.2, `glassDensePlainOpacity` 0.6;
  - Medium: `glassOpacity` 0.1, `glassPlainOpacity` 0.45;
  - High: `glassClearOpacity` 0.04, `glassClearPlainOpacity` 0.3.
  First tried at 0.3/0.85, 0.15/0.7 (glass's old strength) and 0.06/0.5:
  on screen every level was too solid, Low most of all, so all three were
  made clearer and Medium no longer matches the old strength.
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
