# 076: The pill moves on a spring

Status: Accepted

**Context.** Every motion ran on one ease-out curve for one fixed 300 ms:
the morph, a landing, the tab marker, a toast. It looked correct but flat.
What makes a Dynamic Island feel physical is missing from a cubic-bezier
ease-out: going a little past and settling back, growing and shrinking
feeling different, a small change being quick and a big one taking longer,
a thrown shape carrying on. Composition has real springs
(`NaturalMotionAnimation`), but they run until they settle, and the
interaction layer can't replay them exactly, so the click area would drift
off the pill (decision 001).

**Decision.**
- **The spring is ours, in core** (`core/motion.h`): a damped spring
  scaled to settle by the end of its duration (damping 1 − bounce;
  frequency chosen so what's left at the end is under a pixel), with a
  closed form for the position and for the push of a throw. The renderer
  lays it down as `kSpringKeyframes` linear keyframes
  (`platform/spring_motion.h`); the interaction layer evaluates the same
  function each frame. Same curve on both sides, as decision 001 needs.
- **Implicit, still:** the Size/Offset implicit animations use expression
  keyframes `this.StartingValue + (this.FinalValue − this.StartingValue) * p`,
  so an interrupted morph carries on from where it got to, as before.
  Each morph can have its own timing, so `Blob::place` swaps the
  collection before setting a value, and only when it differs.
- **One motion per scene change** (`sceneMotion`), from the old
  `interactiveBounds` to the new: growing overshoots (`morphGrowBounce`,
  ~3%), shrinking doesn't; the time grows with the square root of the
  distance (`morph*Shortest`/`Longest`, `morphFarDistance`). It never
  overshoots below the canvas's bottom (it would be clipped) or past a
  size of 0 (a tucking bubble would go negative).
- **Fling:** while the island or the bubble is carried, the app tracks the
  pointer's speed (`trackPointer`); let go, the landing spring starts with
  it (a lifted island sideways only). Stopped before letting go
  (`flingStaleMs`), nothing carries.
- **Smaller motions** (tab marker, toast in, switch knob, content growing
  in) use `settleEase`, a bezier with a hint of overshoot: they can start
  from wherever they are, which a sampled spring can't.
- **Content arrives** growing from `contentArriveScale` while it fades in,
  each row `contentStaggerMs` after the one above (`staggerDelays`). Only
  new content; a rebuild in place just fades.
- **Content starts with the morph, not a fixed delay** (`revealDelay`): a
  fixed 100 ms was tuned for the old 300 ms ease-out; the spring gets the
  pill most of the way much sooner, so content looked late. It now starts
  when the growing pill is `contentRevealAt` (60%) of the way there, along
  that morph's own curve, and at once when the pill shrinks or keeps its
  size. The renderer keeps it as a moment (`revealAt_`), so the time spent
  before the queued content build (decision 012) counts toward it instead
  of adding to it. Fade, grow-in and stagger are short enough that content
  settles with the pill, not after it.
- **Alert pulse:** a new alert in a pill that keeps its size swells
  (`alertPulseScale`) and springs back, since no morph shows it arrived.
- **Reduced motion:** with Windows' "Animation effects" off
  (`SPI_GETCLIENTAREAANIMATION`, re-read on `WM_SETTINGCHANGE`), morphs are
  `reducedMotionDurationMs` with no overshoot, and there's no stagger,
  fling, pulse or growing in.

**Consequences.** `morphDuration`/`morphEase` now time only the secondary
transitions (ears and corners, the lift's sink, list rows, fades). The
pulse and press scale aren't followed by the interaction layer: both are
small and short. How it feels can only be judged on Windows; every number
is in tokens.json and live-reloads, so tune there. The spring restarts at
rest when interrupted (it doesn't keep its speed); fine at these speeds.
