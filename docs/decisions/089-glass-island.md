# 089: Glass, on Windows 11

Status: Accepted

**Context.** People asked for a "glass" island: the desktop behind it showing
through, blurred, like Windows 11's own surfaces. TR §5 already said a blur
must come from Composition's backdrop brush, not a blur pass of our own. A
Win32 window only gets a backdrop of the desktop behind it on Windows 11
(`DWMWA_USE_HOSTBACKDROPBRUSH`). On Windows 10 the only way is an undocumented
call that blurs the whole window, and our render layer spans the monitor
(decision 026); a separate window per shape couldn't follow the morph, which
runs on the compositor with no app code per frame (TR §5). Refraction like
Apple's Liquid Glass would need a shader over a screen capture: too costly
for an island that should cost nothing at rest, so this is frosted glass.

**Decision.**
- A **Glass** switch in Settings → Appearance, **off by default**: an update
  changes nothing. It's there only where Windows can draw it (Windows 11;
  `allowBackdrop` succeeded on the render layer). Windows 10 has no row, and
  the island stays solid (decision 015: Windows 11-only looks are extras).
- Glass is drawn only while Windows' **Transparency effects** are on too
  (read at start and on `WM_SETTINGCHANGE` "ImmersiveColorSet"). Off, the
  island is solid, as Windows' own surfaces are.
- **Dark islands only** (`withGlass`): with a light theme (a pale island, the
  same test as Settings' Light group), glass stays off even when picked. Its
  dark text over whatever the desktop shows turned grey and hard to read on
  the first Windows run. The switch keeps the pick; a dark theme brings it back.
- How it's drawn, per shape (pill, bubble, floating card):
  - Between the outline twin and the body (decision 018) sits a sprite of
    the **host backdrop** (already blurred by Windows), cut to the twin's live
    snapshot, the same mask the shadow uses, so it follows every morph,
    press and drag on the compositor, ears included. A backdrop can't be a
    mask brush's source, so the cut is a Direct2D Composite effect
    (source-in) built by hand (`platform/glass.cpp`), with no Win2D.
  - Before the cut, the backdrop takes `glassLuminosityColor`'s lightness
    and keeps its own hue (a Blend effect), as Windows' acrylic does. The
    first run without it was lighter than the Start menu beside it: a plain
    tint keeps a bright sky bright. Direct2D's Colour and Luminosity blend
    modes are swapped against their names; WinUI's AcrylicBrush uses
    "Colour" for this step, and so do we. The colour goes in as a Flood
    effect (Win2D's ColorSourceEffect), animated by name
    ("Luminosity.Color"): a colour brush as an effect's source was refused
    with 0x80070057 on the first Windows run. If the brightness step is
    refused anyway, the plain cut of the blur is used, and the log says
    which step failed. Acrylic's grain (a noise texture) is left out.
  - The body's fill is the theme's (or the cover's) colours at
    `glassOpacity` (`islandFill`, core), acrylic's tint over that. The twin keeps them opaque: it's the
    snapshot's alpha, and the solid island wherever the backdrop shows
    nothing. Turning glass on or off fades the fill like any recolour.
  - A rim: `glassRimColor`, `glassRimWidth` wide, a stroke just inside the
    edge, fading in from 40% of the way down so it's brightest along the
    bottom: the island hangs from the screen edge, and the rim never meets
    the ears.
  - The shadow is `glassShadowColor`, fainter: it shows through the island.
  - Ears stop overlapping the body by 1 px (`shape.Overlap` 0): a
    see-through overlap would draw a darker line.
  - The liquid neck takes the see-through colour but no blur: it's thin and
    gone in a moment.

Tokens: `glassOpacity` (a share, not scaled), `glassRimWidth` (a length),
`glassLuminosityColor` (its alpha is how strongly the brightness is evened
out), `glassRimColor`, `glassShadowColor` (themes may set all three). The
defaults follow WinUI's dark acrylic: `#2C2C2C` at 0.96, tint 0.15. `glass` itself is
the app's, like `fadeSpeed`, not tokens.json's. Glass on or off counts as a
recolour (`onlyColorsDiffer`): nothing is rebuilt.

**Not verified on Windows yet** (written in a Linux session): that the
Composite effect accepts the host backdrop and the snapshot as sources, that
the host backdrop draws in our layered, click-through, never-activated
windows, and how it looks. If the effect can't be made, the log says
"glass: no backdrop brush" and the island stays solid. If the backdrop draws
nothing, the solid twin shows under the see-through body: solid again.

**Consequences.** Every shape gets one more sprite once glass has been on;
off, it's faded to nothing. A floating card already open when glass is
switched keeps its look until it's opened again (cards don't refresh on a
tokens change). Tested on Linux: `islandFill` and the token rules
(`tokens_tests.cpp`), the Settings switch (`settings_new_tests.cpp`).
