# 098: The glass glow's lightness follows its hue

Status: Accepted (amends 090)

**Context.** Decision 090 drew every cover's glow at one OKLCH lightness,
`coverDarkGlow` (0.45). Equal lightness isn't equal vividness: each hue is
most vivid at its own lightness, the sRGB gamut's cusp (blue ~0.5, purple
~0.57, red ~0.63, but orange ~0.75, green ~0.86, yellow ~0.9). At 0.45 red,
purple and blue are near their best and glowed well; yellow came out olive
(chroma clipped from 0.22 to ~0.09), cyan and light blue murky teal, green
forest-dark. Dark, dull colours read as a stain, not as light.

**Decision.** The glow's lightness rises with its hue's cusp: from
`coverDarkGlow` for a cusp at or below `coverGlowLiftCusp.x` (0.64: red,
purple, blue unchanged) to `coverDarkGlowLifted` (0.56) at `.y` (0.74) and
up (yellow, green, cyan, orange and brown: a dark orange, it glows amber, not mud), linearly between (light blue, magenta a
little). Only the glow changes: the island, text and accents are as in 090,
and nothing outside glass with music colours is touched.

**Why 0.56.** White text must still read over the glow: at its strongest
(its alpha, at the bottom edge) over glass at its brightest (the luminosity
layer over a white desktop), title text keeps 4.5:1 for every hue. 0.56 is
about the highest that holds for green, yellow and cyan. The old test
checked the glow opaque, which no glow ever is (`glassGlowOpacity` < 1).

**Consequences.** Tested in `tests/cover_tests.cpp`: red, purple, blue glow
exactly as before; yellow, green and cyan lighter; text readable over the
glow of nine hues around the wheel in every dark theme. The three numbers
are live tokens; how it looks needs a Windows run.
