# 024: How the island draws text

Status: Accepted

**Context.** The island's text is light on dark, small (about 11 px for
captions at 100% after `islandScale` 0.85), and drawn with Direct2D into
transparent Composition surfaces. Three problems showed up on screen:
- Captions were the same size as body text, so the hierarchy was gone.
- Small text was blurred.
- All text looked thin: light-on-dark strokes read thinner than
  dark-on-light ones at the same weight.

**Decision.**
- **A type ramp that steps 2 px:** caption 13, body 15, title 17
  (`*FontSize` tokens), plus Display 34 for a timer. The levels read as
  different sizes, not just different greys.
- **Pixel snapping** (`element_visuals.cpp`, `Blob::place`). Element visuals sit
  at whole-pixel offsets and are exactly as big as their whole-pixel
  surfaces, and the pill rests on a whole pixel.
  - Scaled lengths are fractional (12 × 0.85 = 10.2), and a surface stretched
    into a fractional box, or placed between pixels, is resampled, which blurs it.
  - The rounding happens before Blob's change check, so decision 008's
    "never re-set an unchanged offset" still holds.
- **Optical sizes** (`element_painting.cpp`). Segoe UI Variable is cut for
  the size it's shown at. We pick by the final pixel size:
  - *Small* up to 12.5 px: captions at 100%;
  - *Text* above that;
  - *Display* from 28 px.

  The Text cut turns thin and soft at caption sizes. These ranges belong to
  the font, not the design, so they aren't tokens.
- **Weights** (`titleFontWeight` 600, `bodyFontWeight` 500,
  `captionFontWeight` 500, `displayFontWeight` 350). Body and caption are
  medium, not regular, to make up for light-on-dark thinning. The weight is
  also set on Segoe UI Variable's weight axis (`IDWriteTextFormat3::SetFontAxisValues`),
  so values between the named weights apply exactly.
- **Rendering** (`graphics_device.cpp`):
  - Grayscale antialiasing: the surfaces are transparent, so ClearType can't apply.
  - DirectWrite's NATURAL mode, which smooths horizontally only, so
    horizontal strokes stay sharp.
  - The user's own gamma.
  - Grayscale enhanced contrast from `textContrast` (1.5; Windows' usual is
    1), which darkens the antialiased edges so strokes look fuller. The
    rendering params are rebuilt when the token changes.

**Tuning.** All of the above is live in `assets/tokens.json`. Weights are
100..900 and contrast is 0..~3; neither is scaled by the display or
`islandScale`.
- Text still thin: raise `bodyFontWeight`/`captionFontWeight` to 550, or
  `textContrast` to 2.
- Text too heavy or smudgy: lower them to 450, or to 1.
- A bigger size step needs room. Check the card layouts (`expandedArtSize`,
  `expandedSize`, `tileHeight`) against the layout tests.

**Consequences.**
- On Windows 10 (decision 015), Segoe UI has no optical sizes and only some
  named weights. DirectWrite picks the nearest weight it has, so a 500 can
  look like 400 or 600. A missing `IDWriteTextFormat3` or `IDWriteFactory1`
  is skipped: the named weight, and plain enhanced contrast, are used instead.
- During a morph, content moves through fractional positions anyway.
  Snapping is about text at rest.
- Icons (Segoe Fluent Icons / MDL2) keep their normal weight; only text
  styles take the weight tokens.
