#pragma once
// The pill's spring (decision 076) as Composition animations. Composition's
// own springs can't be replayed exactly by the interaction layer (decision
// 001), so the curve from core/motion.h is laid down as kSpringKeyframes
// linear steps: the same curve, sampled.

#include "core/motion.h"
#include "platform/win.h"

namespace cup::platform {

// The body's Size/Offset morph: implicit, from wherever each is now (an
// interrupted morph carries on from there) to what's set, along `motion`.
wuc::ImplicitAnimationCollection springMorph(wuc::Compositor const& c, const Motion& motion);

// An Offset from `from` to `to` along `motion`, leaving `from` at `fling`
// pixels per ms: a dragged shape let go carries on a little the way it was
// thrown, then settles home. Both ends explicit (decision 008).
wuc::Vector3KeyFrameAnimation springOffset(wuc::Compositor const& c, Vec2 from, Vec2 to, const Motion& motion, Vec2 fling);

// Scale swelling to `peak` (about the visual's CenterPoint) and springing
// back to 1 along `motion`: a new alert landing in a pill that keeps its size.
wuc::Vector3KeyFrameAnimation pulseScale(wuc::Compositor const& c, float peak, const Motion& motion);

} // namespace cup::platform
