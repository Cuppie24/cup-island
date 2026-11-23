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
  the same key: "off", "on", "liquid"). Picking Liquid **warns** what it
  costs: a toast over the page ("Liquid glass uses more GPU and battery"),
  and while it's picked the row says so, and that the island is hidden
  from screenshots. Elsewhere it's the switch.
  `Tokens::glassStyle` is the app's, like `glassTransparency`.
- **The picture.** Windows.Graphics.Capture of the island's monitor, on a
  Direct3D device of our own (`platform/liquid_capture`). The island's
  window is left out of captures (`WDA_EXCLUDEFROMCAPTURE`): a picture with
  the island in it would be bent into the island, frame after frame.
- **The shader** (`platform/liquid_shader.h`, one pass, compiled when first
  turned on). For each pixel, its depth inside the nearest shape (a
  rounded rectangle's signed distance, with its flared corners, decision
  025). In a rim `glassLiquidRim` wide it shows what's further **out**, up
  to `glassLiquidBend` px past the edge at the edge itself, growing as a
  cube: what's beside the island wraps into its lip, as round a real
  pane's. Blurred (below), stronger colours
  (`glassLiquidSaturation`), darker (`glassLiquidDim`, so white text reads
  over a white window), the lip lighter (`glassLiquidLipOpacity`). A
  shape on the screen edge has no rim along its top (`liquidShape`, core).
- **The blur is a real one.** Each frame the layer's part of the capture is
  copied to a texture of our own; its quarter-size copy (a mip level) is
  blurred across and then down by a 17-tap Gaussian (`blurps`), into two
  quarter-size textures; the glass reads the result with one tap.
  `glassLiquidBlur` is the Gaussian's sigma in screen pixels (2 to 16).
  Blurring a quarter-size picture costs a sixteenth of the pixels, and a
  blurred picture enlarged is smooth.
- **The bright edge is the compositor's.** The rim's line (decision 089),
  on liquid glass at `glassLiquidHighlightOpacity` (`glassRimAt`, core):
  a vector stroke, antialiased, exactly on the outline through a morph.
- **How it got here: three runs that looked wrong, then a prototype.**
  (1) The bend and a highlight were cut off at the shader's own edge: a
  hard step under the compositor's smooth cut, a staircase. (2) Still
  "pixelated", and the outline ran straight across the flared corners.
  Until then every number was a guess from a Linux session. So the shader
  was copied into a browser (WebGL, a fake desktop of text and colour
  blocks) and looked at, zoomed. It showed what the guesses hadn't: 13
  taps spread over a squeezed, 6 px blur are a comb of dots along the
  edge (the pixels the user saw); a highlight 1.5 px wide drawn by a
  shader is ropey on a curve whatever its falloff; and bending *inward*
  over a blurred picture is invisible: it read as frosted glass. Bending
  outward, with less blur, read as glass at once. (3) Home's widgets with
  a 1.5 px edge at 0.5 looked "weird but not glass": mocked side by side,
  1 px at about 0.2 is what a glass tile's edge looks like. **Look before
  tuning**: a shader's look can be checked in a browser from Linux.
  (4) Still pixelated: the fix in (3) read the picture's smaller copies
  *as* the blur, and a small copy enlarged is a blocky picture, not a
  blurred one. It had looked smooth in the prototype because it was
  judged in views scaled down; zoomed to single pixels it was plainly
  blocky. So: **judge at the pixels** (a nearest-neighbour zoom), and the
  blur became the Gaussian above. And the shiny edges, all the way round
  every widget, read as outlines drawn on: glass catches light on two
  corners, so the shine became a one-pixel hairline round the top left
  and the bottom right, gone in between.
- **What's on the glass shines too.** On liquid glass, a shiny edge: a
  one-pixel stroke just inside the outline, on two corners only: the top
  left and, fainter, the bottom right, fading out along the edge either
  side, `glassLiquidShineOpacity` at its brightest (`liquidShine`, core;
  `platform/shine`);
  fainter on a surface (`glassLiquidSurfaceShineOpacity`). On a filled
  control (a primary button, a toggle that's on), round its circle; on a
  card's large plain controls (media's skips: `liquidRing`, core), round
  their frame; on Home's widgets; on a toast. Not on the small buttons (a
  row's, the page row's): a ring round each would be noise. Nor on a menu,
  which has its own edge and is opaque. Plain shapes, no shader. Not on
  frosted glass, and not in providers' windows.
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

**Consequences.** `platform/liquid_glass` (+ `_build`), `liquid_capture`,
`liquid_shader.h`; `Blob::setLiquid`; `core/liquid_glass`. Tested on Linux:
the shapes and the spring (`tests/motion_tests.cpp`), the tokens
(`tests/tokens_tests.cpp`), the Settings pick (`tests/settings_new_tests.cpp`).
