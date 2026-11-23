# 165: Liquid glass: a second glass, from a captured picture of the desktop

Status: Accepted; built, not yet run on Windows (amends 089, which ruled refraction out; 096's Glass page)

**Context.** The island's glass (decision 089) is frosted: Windows' blurred
desktop at one brightness under a tint. The user asked for liquid glass
like the iPhone's, on the island only (providers' windows keep Mica,
decision 155), told us to set the earlier decisions aside, and asked what
it costs. 089 had ruled it out: "a shader over a screen capture: too
costly for an island that should cost nothing at rest".

**Tried first: a lens on the compositor.** Without a capture, all Windows
gives is the host backdrop, already blurred. A band inside the edge showed
it again, magnified (a 2D transform effect), in three steps of opacity. On
screen it looked awful: the steps showed as stripes, a blurred picture
moved a few pixels looks no different, and nothing about it read as the
iPhone's glass. Removed. What reads as liquid glass is a *sharp* desktop
bending smoothly at the lip, and Composition has neither a sharp desktop
nor a displacement effect.

**Decision.** Liquid glass is a **second style**, picked by the user; the
frosted glass stays as it was and stays the default.
- **Settings → Glass**: where liquid glass can be drawn, "Glass effect" is
  three picks in place of the switch: **Off, Frosted, Liquid** (saved in
  the same key: "off", "on", "liquid"). The row says what Liquid costs:
  the island is hidden from screenshots. Elsewhere it's the switch.
  `Tokens::glassStyle` is the app's, like `glassTransparency`.
- **The picture.** Windows.Graphics.Capture of the island's monitor, on a
  Direct3D device of our own (`platform/liquid_capture`). The island's
  window is left out of captures (`WDA_EXCLUDEFROMCAPTURE`): a picture with
  the island in it would be bent into the island, frame after frame.
- **The shader** (`platform/liquid_shader.h`, one pass, compiled when first
  turned on). For each pixel, its depth inside the nearest shape (a
  rounded rectangle's signed distance). In a rim `glassLiquidRim` wide it
  shows what's up to `glassLiquidBend` px further in, growing as a square:
  gentle inside, steepest at the edge, no steps. Then a little blur
  (`glassLiquidBlur`), stronger colours (`glassLiquidSaturation`), darker
  (`glassLiquidDim`, so white text reads over a white window), and a
  highlight on the edge lit from the top left (`glassLiquidHighlightOpacity`).
  A shape on the screen edge has no rim along its top (`liquidShape`, core).
  Nothing in the shader changes *at* the edge: pixels past it are drawn as
  if on it, and the highlight fades both ways. The first run cut the bend
  and the highlight off at the shader's own idea of the edge, a hard step
  that showed through the compositor's smooth cut as a staircase; and the
  bend then grew as a quarter circle, infinitely steep at the edge, which
  squeezed the desktop into single glittering pixels. Now it grows as a
  square, and the blur widens with the squeeze.
- **Controls shine too.** On liquid glass a filled control on the island
  (a primary button, a toggle that's on) gets a shiny edge: a stroke just
  inside its circle, brightest at the top left, dim across the middle,
  bright again at the bottom right, `glassLiquidShineOpacity` at its
  brightest (`liquidShine`, core; `platform/shine`). Plain shapes, no
  shader. Not on frosted glass, and not in providers' windows.
- **Shown by the compositor.** The shader draws into a swap chain as large
  as the render layer; each Blob's glass sprite shows it through a mask
  brush cut by the same outline snapshot as before (decisions 018, 133),
  under the same tint (097), its picture moved back by the sprite's own
  offset so each pixel stays where the desktop's is. So the edge is exact
  at every frame of a morph; the shader only needs to know roughly where
  the shapes are, which it works out from the same spring
  (`morphRect`, core).
- **Fallbacks.** Anything refused (no hardware device, the window can't be
  left out of captures, the capture, a shader) is logged with its step and
  the frosted glass stays. Lost while running (a GPU reset): the frosted
  glass, until Liquid is picked again.

**What it costs, and why it's a pick.**
- GPU and a little CPU **whenever the screen changes** (a video anywhere
  on the monitor, a moving window) and while the island morphs: a copy of
  the screen by Windows, one shader pass over the island's pixels by us.
  Nothing while the screen is still; nothing while the island is hidden
  under a fullscreen app (the capture is paused).
- The glass is **a frame or so behind** what moves under it.
- The island is **missing from screenshots, recordings and screen shares**
  while Liquid is on.
- Protected video shows black behind it, as in any capture.
- Windows 11 only (a capture without a border round the screen).

**Not verified on Windows yet** (written in a Linux session): everything
in `platform/liquid_*`. Above all that a layered, click-through window can
be left out of captures, that the capture runs without a border or a
prompt, and that the picture lines up with the desktop to the pixel.

**The log.** Every glass line starts `glass:` (liquid's: `glass: liquid:`),
and every refusal goes through `logGlassError`: what failed, the step, the
code and Windows' own message. The silent ways out (no backdrop on this
Windows, no snapshot, a setting that can't be read) say so too.

**Consequences.** `platform/liquid_glass`, `liquid_capture`,
`liquid_shader.h`; `Blob::setLiquid`; `core/liquid_glass`. Tested on Linux:
the shapes and the spring (`tests/motion_tests.cpp`), the tokens
(`tests/tokens_tests.cpp`), the Settings pick (`tests/settings_new_tests.cpp`).
