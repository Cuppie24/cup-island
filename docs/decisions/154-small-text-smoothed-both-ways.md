# 154: Small text is smoothed both ways

Status: Accepted (amends 024). The look needs a Windows run.

**Context.** Decision 024 drew small text with DirectWrite's NATURAL mode:
antialiased horizontally only, so horizontal strokes stay sharp. That trade
assumes ClearType, which our transparent surfaces can't use. In grayscale,
NATURAL leaves every curve and diagonal unsmoothed vertically: the round
tops and bottoms of "S", "e", "p", "g" stair-step, and text looks pixelated,
most visibly dark text on the windows' light backgrounds (Settings' page
titles, labels and captions). Decision 107 had seen the same on big digits.

**Decision.** Small text uses `DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC`
(`graphics_device.cpp`): smoothed both ways, still hinted to pixel rows so
baselines and horizontal stems stay put. It is the mode Windows itself uses
for grayscale text. Everything else in 024 (gamma, `textContrast`, weights,
optical sizes, snapping) is unchanged; big text keeps OUTLINE (107).

**Consequences.**
- Curves and diagonals are smooth; horizontal strokes may look a touch
  softer at caption sizes. If small text now reads too light, raise
  `textContrast` (024's tuning notes).
- NATURAL_SYMMETRIC has existed since DirectWrite's first version, so
  Windows 10 1809 (decision 015) is unaffected.
