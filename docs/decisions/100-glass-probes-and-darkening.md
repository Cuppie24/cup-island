# 100: Glass: probes of what Windows takes, and a darkened blur

Status: Accepted; built, not yet run on Windows (amends 089)

**Context.** Over a bright window the glass island turned light grey and
its white text was hard to read. The log said why: Windows refused
acrylic's even brightness (the Colour blend, decision 089) at the effect
factory, `0x80070057`, every time, so the island ran on the plain blur,
whose brightness nothing keeps in hand. It had also refused a flood colour
and a Multiply blend. The only effect it ever took from us was the cut to
the island's shape (Composite). Windows lists all of these as supported,
so the refusal is more likely in how our hand-written effects describe
themselves (nesting, how a setting is passed) than in the effects.

**Decision.**
- **Probes, once at startup, logged only** (`glass: probe: …: yes/no`):
  a composite inside the cut (is it nesting?), a Multiply blend and the
  acrylic blend alone (is it the blend, or the blend in the cut?), an
  effect with no settings (luminance to alpha: is it passing a setting?),
  and the colour matrix alone. One run says which guess is right.
- **A darkened blur between acrylic and plain.** If Windows refuses the
  blend, the backdrop goes through a colour matrix first: every channel
  times `glassDarkenScale` (0.35), plus `glassDarkenLift` (0.08). That
  squeezes it into a darker range (white comes out at 43%, black at 8%)
  but keeps its light and dark shapes, so the glass still reacts to
  what's behind it, unlike acrylic's one brightness. The clearer
  opacities (`glassOpacity`…, not the Plain ones) go over it. The two
  numbers are live: a change makes the brush again.
- The order is acrylic, then darkened, then plain; the log says which.
- One effect type (`Effect` in platform/glass.cpp) now describes every
  effect: its inputs in order and its properties by Direct2D's index.

**Consequences.** The next Windows run's `glass:` lines say which effects
Windows takes, and so what to fix if the darkened blur is refused too.
The two numbers are unitless (tested in `tests/tokens_tests.cpp`).
