# 172: Liquid glass is drawn only for an open card

Status: Accepted; built, not yet run on Windows (amends 165)

**Context.** Liquid glass (decision 165) bends a captured picture of the
desktop. The capture is its cost, and that cost has nothing to do with the
island's size: Windows copies the whole monitor each time anything on it
changes, for a 36-pixel pill as for a card. The island is collapsed most of
the time, and on a pill that low the bend at the edge hardly shows. The
frosted glass (decision 089) costs nothing: Windows has blurred the desktop
already. The user asked whether the pill could use it, and whether
switching between the two is itself expensive.

**Decision.**
- **Collapsed, the pill is the frosted glass.** Liquid glass's picture is
  drawn only while a card is open (`liquidDrawn`, core): the pill expanded.
  The Split bubble is never a card, so it's always frosted.
- **Its shiny edges stay, collapsed too** (the user's condition):
  everything that says "liquid glass" without the picture goes by
  `liquidPicked` (core), not by what is drawn: the rim's brighter line
  (`glassRimAt`), the hairlines on controls, widgets and toasts
  (`liquidShine`).
- **Switching.** The device, the swap chain and the shaders are kept; only
  the capture starts and stops (`LiquidGlass::setActive`), as it already
  did for an island hidden under a fullscreen app. Starting it is set-up,
  once per opening, not a cost per frame.
- **A crossfade, not a cut.** The frosted glass is always there; liquid
  glass is a sprite of its own over it, under the same tint and cut by the
  same outline (`Blob::setLiquid`, `showLiquid`). Opening: the card opens
  frosted, and fades to liquid (hoverFadeDuration) when the first fresh
  picture is up (`onReady`), a few frames in. Collapsing: it fades back at
  once, and the capture stops; the last picture stays in the swap chain
  under the fade.
- While collapsed the island is in screenshots and recordings again: its
  window is left out of captures only while the capture runs.

**Consequences.** Liquid glass costs something only while a card is open.
The two glasses differ in blur and brightness, so the change shows a little
as a card opens; the crossfade is what keeps it from being a jump. Tested
on Linux: `liquidPicked`, `liquidDrawn` (`tests/motion_tests.cpp`). The
switch itself, and how the crossfade looks, need a Windows run.
