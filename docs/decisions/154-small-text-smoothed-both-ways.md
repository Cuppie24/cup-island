# 154: Small text is smoothed both ways

Status: Accepted (amends 024). Released in 0.12.13.

**Context.** Text in the windows looked pixelated: Settings' page title
("Updates"), its list labels ("Appearance") and its captions. The letter
"s" was the worst. Decision 024 drew all small text with DirectWrite's
NATURAL mode, which antialiases horizontally only and keeps glyphs on whole
pixel rows. That trade is made for ClearType, which smooths edges itself.
Our surfaces are transparent, so ClearType can't run and text is
grayscale-antialiased (024). With grayscale, nothing smooths the vertical
direction: every curve's top and bottom turns into hard steps. "s" is
almost nothing but such curves (two bowls and a diagonal spine), so it
showed it most, then "e", "o", "p", "g". Dark text on the windows' light
backgrounds (decision 150) makes the steps easier to see than light text on
the island's dark glass. Decision 107 had met the same flaw on the
timer's big digits and moved them to OUTLINE, but left small text alone.

**Decision.** Small text uses `DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC`
(`GraphicsDevice::updateTextParams`, `graphics_device.cpp`). It antialiases
both ways and still keeps glyphs on pixel rows, so baselines and horizontal
stems stay sharp. It is the mode Windows itself uses for grayscale text.
- One change for the island and the windows: they share `GraphicsDevice`.
- Big text (Hero, Display, the Dial's time) stays on OUTLINE (107).
- Everything else in 024 stays as it is: the user's gamma, `textContrast`,
  weights, optical sizes and pixel snapping.

(Not chosen: ClearType. It needs an opaque surface to blend against, and our
Composition surfaces are transparent. Turning `textContrast` down: it only
softens how dark the edge pixels are, not the missing vertical smoothing.
OUTLINE for all text: unhinted, small text drifts off pixel rows and reads
blurry.)

**Consequences.**
- Curves and diagonals are smooth. Horizontal strokes may look a touch
  softer at caption sizes. If small text now reads too light, raise
  `textContrast` from 1.5 toward 2 in `assets/tokens.json` (live; 024's
  tuning notes).
- NATURAL_SYMMETRIC has been in DirectWrite since its first version, so
  Windows 10 1809 (decision 015) needs no fallback.
- No core change: the rendering mode lives in `platform/`, which only a
  Windows run shows. To check it, look at an "s" in Settings at 100% scaling,
  where the steps were most visible.
