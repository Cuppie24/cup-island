# 165: Liquid glass: a lens along the island's edge, on the compositor

Status: Accepted; built, not yet run on Windows (amends 089, which ruled refraction out)

**Context.** The island's glass (decision 089) is frosted: the blurred
desktop at one brightness under a tint. The user asked for it remade as
liquid glass, like Apple's, on the island only (providers' windows keep
Mica, decision 155), and asked whether that can be done without hurting
performance. 089 said refraction "would need a shader over a screen
capture: too costly". That part still holds: capturing the screen
(Windows.Graphics.Capture, Desktop Duplication) copies the monitor on every
frame anything changes, lags a frame behind what it bends, draws a capture
border on Windows 10, blacks out protected video, and to keep the island
out of its own picture the island would have to be hidden from every
screenshot and screen share. So no capture.

What reads as liquid glass, though, is mostly the **edge**: a thick pane
bends what's behind it near its rim and lets it through brighter there.
That much Composition can do alone, with the host backdrop the glass
already uses.

**Decision.** A **lens**, over the glass and under the twin's tint
(decision 097), on the pill and the bubble:
- In a band `glassLensWidth` (10) wide inside the edge, the blurred desktop
  shows a second time, **without** acrylic's even brightness: over a bright
  window the lip lights up, over a dark one it all but disappears. Drawn
  `glassLensOpacity` (0.45) strong, strongest at the edge, fading inward in
  three steps.
- In that band the desktop is **pulled in from the edge** by
  `glassLensDepth` (14 px): Direct2D's 2D transform effect over the host
  backdrop, magnifying about the middle of the top edge by just enough that
  each side and the bottom show what's that far in. Magnified, never
  shrunk: it only ever reads pixels the island already covers. Its matrix
  is an expression on the island's live size, so the depth holds through
  every morph, press and drag with no app code per frame (TR §5).
- The band is a ring of strokes on the island's outline, snapshotted like
  the outline twin (a `CompositionVisualSurface`, BorderMode Soft, decision
  133) and re-rendered only when the shape changes. Like the rim it is
  clear up the sides until 40% of the way down: the island hangs from the
  screen edge, and nothing should bend where it meets the ears.
- Chained brushes, not nested effects (decision 100): the transform is a
  brush of its own, the cut (Composite source-in, the one effect Windows
  has always taken) takes it as input.
- **If Windows refuses** the transform, or animating its matrix, the band
  shows the desktop where it is: the bright lip without the bending. If it
  refuses even that, the glass is as before. The log says which
  (`glass: lens ready, bending` / `a lip without bending` / `no lens`).
- Any of the three tokens at 0 turns the lens off. The Glass switch,
  Transparency effects and light themes rule it as they rule the glass.

**What it costs.** Nothing at rest that the glass didn't: Windows already
blurs the desktop behind the island for the glass; the lens samples that
same backdrop once more over a thin band (two small shader passes, a few
thousand pixels). No timer, no capture, no app code per frame; one more
small offscreen surface per shape, redrawn only while the shape changes.

**What it isn't.** The desktop Windows hands us is already blurred, so the
bend is soft: colours and light shift at the lip, but no sharp, readable
image of what's behind curls round the edge as on iOS. That would need the
screen capture ruled out above.

**Not verified on Windows yet** (written in a Linux session): that
Composition takes our hand-described 2D transform over a host backdrop and
animates its matrix, that a snapshot's source may be a visual outside the
tree (Lottie-Windows does this), and how the lip looks over bright and dark
desktops. The three tokens are live: tune them with the app running.

**Consequences.** `platform/blob_lens.cpp`, `lensBackdrop` in
`platform/glass.cpp`; effects can name properties for animation
(`platform/effects.h`). Tested on Linux: `glassLensShare` and the tokens'
units (`tests/tokens_tests.cpp`).
