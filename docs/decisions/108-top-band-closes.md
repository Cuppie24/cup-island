# 108: The top of an expanded island always closes it

Status: Accepted (amends 030, 046, 071: where a click on a card's empty space does nothing, the top band still closes). Core tested; the band's press and glow on Windows need a run.

**Context.** A click on a card's empty space closes it, except on Home, on
a card that keeps open (Settings, the timer: `Content::keepOpen`) and when
pinned (decision 046). So whether a click closes depended on which page was
shown. The user asked for one place that always works: the top of the
island, where it hangs from the screen's edge.

**Decision.**
- **The top band** (`Scene::collapseZone`): the expanded island's top
  `collapseZoneHeight` (12) px, its whole width, inside the top padding
  (18). A click there sends `TopClicked`, which collapses in any case: Home,
  a card that keeps open, pinned (the pin goes, as with any close, decision 046).
- **It wins over what's under it.** Tabs on top reach up to the card's edge
  to be easy to hit (decision 047); in the band the band takes the press
  instead (`inCollapseZone`, checked before tabs and buttons). A press there
  squashes the island like any closing click; let go off it, nothing happens.
- **It glows under the pointer** (`platform/top_glow`): a gradient down
  from the top edge, `collapseGlowHeight` (28) tall, the theme's text colour
  at `collapseGlowOpacity` (0.12) fading to clear: light on a dark island, a
  shade on a light one. Nothing under the band lights up meanwhile. It
  spreads in from the top edge's middle (`collapseGlowSpread` of its size)
  to its full size as it fades in, over `collapseGlowDuration` (280 ms),
  and shrinks back as it fades out; with reduced motion it only fades.

**Not taken.** A visible handle or bar: the glow shows the band only when
it's about to be used, and the island stays clean otherwise. The whole
padding (18 px): the corner button sits at 18, and its click area reaches
a little above it.

**Consequences.** `tests/scenarios/top_band.scenario` holds the rule.
