#pragma once
// How things move (decision 076): the pill's morph is a spring that settles
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
    // How early in its time the spring does its moving (morphSpringSettle):
    // higher is snappier and more front-loaded.
    float settle = 6;
    bool operator==(const Motion&) const = default;
};

// Composition runs a spring as this many linear steps (plus the start): fine
// enough that the steps don't show at the morph's speeds.
inline constexpr int kSpringKeyframes = 24;

// Progress along `motion`'s spring from 0 at t = 0 to exactly 1 at t = 1 (a
// fraction of the duration). With bounce > 0 it goes past 1 once, then settles.
float springProgress(const Motion& motion, float t);

// The same spring started at rest but moving at unit speed (one whole
// duration's worth of distance per duration): 0 at both ends. Added on top of
// springProgress, scaled by a starting velocity, it carries a throw (fling).
float springCarry(const Motion& motion, float t);

// How far past its end a spring with this bounce goes, as a share of the
// distance (0.25 -> about 3%).
float overshootOf(float bounce);

// The motion for a morph of the pill's bounds from `from` to `to`: growing
// overshoots a little, shrinking doesn't. Its time grows with the square root
// of how far it goes (morphDistance), with no edge but a safety ceiling.
// Overshoot is trimmed so the pill never grows past the bottom of the canvas.
Motion morphMotion(Rect from, Rect to, const Tokens& t);

// How far a morph goes, in pixels: the size change's diagonal (growing both
// ways counts more than one) plus how far its top centre moves (a pill growing
// about its centre doesn't move; one sliding along the edge does).
float morphDistance(Rect from, Rect to);

// `motion` for a shape heading to `size`: no overshoot when it tucks away to
// nothing, where going past would mean a negative size.
Motion towardSize(Motion motion, Vec2 size);

// The scene just shown is a new alert in a pill that keeps its size: it
// swells and springs back (alertPulseScale), so the change isn't missed.
bool alertPulses(const Scene& was, const Scene& next, const Tokens& t);

// How long after a morph from `from` to `to` starts new content should
// start arriving: when the growing pill is contentRevealAt of the way there
// along `morph`. At once when it shrinks, doesn't change, or motion is reduced.
Ms revealDelay(const Motion& morph, Rect from, Rect to, const Tokens& t);

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
    Ms reveal{};  // new content starts arriving this long after the morph starts (revealDelay)
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
