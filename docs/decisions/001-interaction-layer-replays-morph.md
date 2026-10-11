# 001: The interaction layer replays the morph instead of reading it

Status: Accepted

**Context.** The clickable window must match the visible pill every frame
during a morph (TR §9.1). But Composition never reports animated values back
to the app. Reading an animating `Visual.Size` returns the last value the app
set, and `AnimationController.Progress` stayed at 0 for the whole run
(verified by polling during a morph).

**Decision.** The app replays the same morph itself: same endpoints, duration
and easing (`cubicBezierEase` with the curve from `tokens.h`). It's clocked
from the compositor's own frame ticks (`DCompositionWaitForCompositorClock` on
a helper thread that posts one message per frame), and it snaps to the exact
target at the end. A new morph starts from wherever the window is now, which
matches where the compositor picks up an interrupted animation.

**Consequences.** Renderer and follower must use identical timing, which is
why both read `tokens::kMorphDuration` / `kMorphEase*`. The helper thread
sleeps when no morph runs, so idle cost stays zero. If Microsoft adds a way
to read live animated values, this can go.
