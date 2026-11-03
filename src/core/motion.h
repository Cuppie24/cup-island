#pragma once
// How things move (decision 075): the pill's morph is a spring that settles
// within a set time, picked per morph from what changes. Renderer and
// interaction layer both run these same curves, so the clickable window stays
// on the visible pill (decision 001). Pure: time is a fraction, or passed in.

#include "core/geometry.h"
#include "core/layout.h"
#include "core/tokens.h"

#include <vector>

namespace cup {

// One morph: how long it runs and how far it overshoots (0 = not at all).
struct Motion {
    Ms duration{ 300 };
    float bounce = 0;
    bool operator==(const Motion&) const = default;
};

// Composition runs a spring as this many linear steps (plus the start): fine
// enough that the steps don't show at the morph's speeds.
inline constexpr int kSpringKeyframes = 24;

// Progress along a spring from 0 at t = 0 to 1 at t = 1 (a fraction of the
// duration). With bounce > 0 it goes past 1 once, then settles; it's settled
// (within a fraction of a pixel) by t = 1, where the caller snaps to the end.
float springProgress(float bounce, float t);

// The same spring started at rest but moving at unit speed (one whole
// duration's worth of distance per duration): 0 at both ends. Added on top of
// springProgress, scaled by a starting velocity, it carries a throw (fling).
float springCarry(float bounce, float t);

// How far past its end a spring with this bounce goes, as a share of the
// distance (0.25 -> about 3%).
float overshootOf(float bounce);

// The motion for a morph of the pill's bounds from `from` to `to`: growing
// overshoots a little, shrinking doesn't; farther takes longer. Overshoot is
// trimmed so the pill never grows past the bottom of the canvas.
Motion morphMotion(Rect from, Rect to, const Tokens& t);

// `motion` for a shape heading to `size`: no overshoot when it tucks away to
// nothing, where going past would mean a negative size.
Motion towardSize(Motion motion, Vec2 size);

// The scene just shown is a new alert in a pill that keeps its size: it
// swells and springs back (alertPulseScale), so the change isn't missed.
bool alertPulses(const Scene& was, const Scene& next, const Tokens& t);

// New content arriving, top row first: each row's delay after the first, by
// each element's top (in the order given). Rows past contentStaggerRows wait
// as long as the last one.
std::vector<Ms> staggerDelays(const std::vector<float>& tops, const Tokens& t);

// The curve small things settle on (settleEase), or the plain ease-out
// without its overshoot when motion is reduced.
inline Bezier settleCurve(const Tokens& t) { return t.reducedMotion ? t.morphEase : t.settleEase; }

// How one scene change moves, for the renderer and the interaction layer alike.
struct SceneMotion {
    Motion morph; // the pill's bounds (interactiveBounds) from the old scene's to the new
    Vec2 fling;   // what was just let go carries this (pixels per ms) into its landing
    bool pulse = false;
};
// `pointer`: the drag's speed if one ends here (flingVelocity). Only what was
// let go carries it: a lifted island sideways (it lands on the edge), the
// bubble any way.
SceneMotion sceneMotion(const Scene& was, const Scene& next, Vec2 pointer, const Tokens& t);

// The pointer's recent speed while something is dragged, for a fling.
struct PointerTrack {
    bool started = false;
    Vec2 at;       // the last point taken in
    Ms when{};     // ...and when
    Vec2 velocity; // pixels per ms, smoothed over the last few moves
    bool measured = false; // velocity has been measured at least once
};
PointerTrack trackPointer(PointerTrack track, Vec2 at, Ms when, const Tokens& t);

// The speed something let go `now` carries into its landing: none if the
// pointer had stopped (flingStale) or motion is reduced; capped at flingMaxSpeed.
Vec2 flingVelocity(const PointerTrack& track, Ms now, const Tokens& t);

} // namespace cup
