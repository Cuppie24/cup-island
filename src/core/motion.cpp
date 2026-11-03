#include "core/motion.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cup {
namespace {

// A damped spring on a time scale of one duration (decision 080). Its
// damping ratio ζ is 1 - bounce; its natural frequency ω is settle / ζ, so
// the envelope e^(-ζωt) is e^(-settle) by t = 1. A high settle does nearly
// all its moving early (8.5: half the way in an eighth of the time), which
// made big morphs feel rushed; a lower one spreads it out. What's left at
// t = 1 is scaled away (springProgress), so the end never jumps.
constexpr float kMaxBounce = 0.9f; // past this it rings for the whole duration
constexpr float kCritical = 1e-3f; // bounce below this is treated as none: the formulas below divide by it

float dampingOf(float bounce) { return 1.0f - std::clamp(bounce, 0.0f, kMaxBounce); }

// The spring released from 1 at rest (x0 = 1, v0 = 0), or from 0 at unit
// speed (x0 = 0, v0 = 1): its displacement from the end at time t.
float displacement(const Motion& m, float t, bool fromRest)
{
    float zeta = dampingOf(m.bounce), omega = std::max(m.settle, 1.0f) / zeta;
    float decay = std::exp(-zeta * omega * t);
    if (1.0f - zeta < kCritical) return fromRest ? (1.0f + omega * t) * decay : t * decay;
    float omegaD = omega * std::sqrt(1.0f - zeta * zeta); // how fast it rings
    if (fromRest) return decay * (std::cos(omegaD * t) + zeta * omega / omegaD * std::sin(omegaD * t));
    return decay * std::sin(omegaD * t) / omegaD;
}

// The inverse of overshootOf: the bounce that goes past by `share`.
float bounceFor(float share)
{
    if (share <= 0) return 0;
    if (share >= overshootOf(kMaxBounce)) return kMaxBounce;
    float l = std::log(share);
    return 1.0f - -l / std::sqrt(std::numbers::pi_v<float> * std::numbers::pi_v<float> + l * l);
}

float area(Rect r) { return r.width * r.height; }

} // namespace

float springProgress(const Motion& m, float t)
{
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    // Stretched so it ends exactly at 1: a low settle leaves a little to go
    // (about 1.7% critically damped at settle 6), which would show as a snap.
    return (1.0f - displacement(m, t, true)) / (1.0f - displacement(m, 1.0f, true));
}

float springCarry(const Motion& m, float t)
{
    if (t <= 0 || t >= 1) return 0;
    return displacement(m, t, false) - t * displacement(m, 1.0f, false); // ...and this ends at 0
}

float overshootOf(float bounce)
{
    float zeta = dampingOf(bounce);
    if (1.0f - zeta < kCritical) return 0;
    return std::exp(-std::numbers::pi_v<float> * zeta / std::sqrt(1.0f - zeta * zeta));
}

float morphDistance(Rect from, Rect to)
{
    float resize = std::hypot(to.width - from.width, to.height - from.height);
    float move = std::hypot((to.x + to.width / 2) - (from.x + from.width / 2), to.y - from.y);
    return resize + move;
}

Motion morphMotion(Rect from, Rect to, const Tokens& t)
{
    if (t.reducedMotion) return { t.reducedMotionDuration, 0, t.morphSpringSettle };
    // Equal areas (a move along the edge, a landing) settle like growing: a
    // little past and back reads as arriving. Shrinking never goes past.
    bool shrinks = area(to) < area(from);
    Ms shortest = shrinks ? t.morphShrinkShortest : t.morphGrowShortest;
    Ms reference = shrinks ? t.morphShrinkReference : t.morphGrowReference;
    Ms ceiling = std::max(shrinks ? t.morphShrinkMax : t.morphGrowMax, shortest);
    // Square root through two points: no time at all for no distance beyond
    // the shortest, and `reference` at morphReferenceDistance. Each pixel adds
    // a little less than the one before, so a big morph gets more time without
    // dragging; the ceiling is only a guard (a huge display), not where morphs end up.
    float share = t.morphReferenceDistance > 0 ? std::sqrt(morphDistance(from, to) / t.morphReferenceDistance) : 1.0f;
    float ms = static_cast<float>(shortest.count()) + static_cast<float>((reference - shortest).count()) * share;
    Motion motion{ std::clamp(Ms{ std::lround(ms) }, shortest, ceiling),
                   std::clamp(shrinks ? t.morphShrinkBounce : t.morphGrowBounce, 0.0f, kMaxBounce), t.morphSpringSettle };
    // Going past the end downward must stay on the canvas: the render layer
    // clips anything below it (decision 016).
    float grownDown = (to.y + to.height) - (from.y + from.height), room = t.canvas.y - (to.y + to.height);
    if (motion.bounce > 0 && grownDown > 0 && overshootOf(motion.bounce) * grownDown > room)
        motion.bounce = room > 0 ? std::min(motion.bounce, bounceFor(room / grownDown)) : 0.0f;
    return towardSize(motion, { to.width, to.height });
}

Motion towardSize(Motion motion, Vec2 size)
{
    if (size.x <= 0 || size.y <= 0) motion.bounce = 0;
    return motion;
}

bool alertPulses(const Scene& was, const Scene& next, const Tokens& t)
{
    if (t.reducedMotion || next.mode != Mode::Alert || next.size != was.size) return false; // a morph shows the change itself
    return was.mode != Mode::Alert || next.alertPosted != was.alertPosted;
}

Ms revealDelay(const Motion& morph, Rect from, Rect to, const Tokens& t)
{
    if (t.reducedMotion || from == to || area(to) < area(from)) return Ms{ 0 };
    // The first moment the spring gets that far (it can pass it again on the
    // way back from an overshoot); fine steps, since a frame is ~1/20 of a morph.
    constexpr int kSteps = 200;
    for (int i = 0; i <= kSteps; ++i) {
        float at = static_cast<float>(i) / kSteps;
        if (springProgress(morph, at) >= t.contentRevealAt)
            return Ms{ std::lround(at * static_cast<float>(morph.duration.count())) };
    }
    return morph.duration;
}

std::vector<Ms> staggerDelays(const std::vector<float>& tops, const Tokens& t)
{
    std::vector<Ms> delays(tops.size(), Ms{ 0 });
    if (t.reducedMotion || tops.empty() || t.contentStaggerRows == 0) return delays;
    // A row is a top within a pixel of another's: things side by side in a
    // row (an icon and its title) arrive together.
    std::vector<float> rows(tops);
    std::sort(rows.begin(), rows.end());
    std::vector<float> distinct;
    for (float top : rows)
        if (distinct.empty() || top - distinct.back() > 1.0f) distinct.push_back(top);
    for (std::size_t i = 0; i < tops.size(); ++i) {
        std::size_t row = 0;
        while (row + 1 < distinct.size() && tops[i] - distinct[row + 1] > -1.0f) ++row;
        delays[i] = t.contentStagger * static_cast<long long>(std::min(row, t.contentStaggerRows - 1));
    }
    return delays;
}

SceneMotion sceneMotion(const Scene& was, const Scene& next, Vec2 pointer, const Tokens& t)
{
    Rect from = interactiveBounds(was), to = interactiveBounds(next);
    SceneMotion motion{ morphMotion(from, to, t), {}, alertPulses(was, next, t) };
    motion.reveal = revealDelay(motion.morph, from, to, t);
    auto carried = [](const Scene& s) { return s.bubble && s.bubble->dragCentre; };
    if (was.lifted && !next.lifted) motion.fling = { pointer.x, 0 };
    else if (carried(was) && !carried(next)) motion.fling = pointer;
    return motion;
}

PointerTrack trackPointer(PointerTrack track, Vec2 at, Ms when, const Tokens& t)
{
    if (!track.started) return { true, at, when, {}, false };
    Ms gap = when - track.when;
    // Two moves in the same millisecond: wait for the next, which measures
    // from the same baseline over a real time.
    if (gap <= Ms{ 0 }) return track;
    float ms = static_cast<float>(gap.count());
    Vec2 now{ (at.x - track.at.x) / ms, (at.y - track.at.y) / ms };
    // Smoothed, so one jittery move doesn't decide the throw; the first, or
    // one after a pause (the old speed is stale), stands alone.
    constexpr float kLatest = 0.6f;
    Vec2 velocity = !track.measured || gap > t.flingStale ? now : lerp(track.velocity, now, kLatest);
    return { true, at, when, velocity, true };
}

Vec2 flingVelocity(const PointerTrack& track, Ms now, const Tokens& t)
{
    if (!track.started || t.reducedMotion || now - track.when > t.flingStale) return {};
    float speed = std::hypot(track.velocity.x, track.velocity.y);
    if (speed <= t.flingMaxSpeed || speed <= 0) return track.velocity;
    float cap = t.flingMaxSpeed / speed;
    return { track.velocity.x * cap, track.velocity.y * cap };
}

} // namespace cup
