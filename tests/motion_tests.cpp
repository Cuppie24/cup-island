// The pill's spring and the timing around it (decision 080, core/motion.h).
// The renderer and the interaction layer both run these curves (decision 001),
// so their ends and overshoot are what keeps the click area on the pill.

#include "core/motion.h"
#include "expect.h"

#include <algorithm>
#include <cmath>

namespace cup::test {
namespace {

bool near(float a, float b, float within = 0.005f) { return std::abs(a - b) <= within; }

// A spring with the default settle, for the curve's own shape.
Motion spring(float bounce) { return { Ms{ 300 }, bounce, Tokens{}.morphSpringSettle }; }

// The highest point a spring reaches on its way, sampled like a keyframe run (finer).
float peakOf(float bounce)
{
    float peak = 0;
    for (int i = 0; i <= 1000; ++i) peak = std::max(peak, springProgress(spring(bounce), i / 1000.0f));
    return peak;
}

void springTests()
{
    for (float bounce : { 0.0f, 0.1f, 0.22f, 0.5f }) {
        expect(springProgress(spring(bounce), 0) == 0 && springProgress(spring(bounce), 1) == 1, "a spring starts at 0 and ends at 1");
        // Settled by the end: the snap there is under a pixel even across a whole card.
        expect(near(springProgress(spring(bounce), 0.999f), 1, 0.002f), "a spring has settled by the end of its time: no jump");
        expect(springCarry(spring(bounce), 0) == 0 && near(springCarry(spring(bounce), 0.999f), 0, 0.001f), "a fling's push starts and ends at nothing");
        expect(springCarry(spring(bounce), 0.1f) > 0, "a fling pushes the way it was thrown");
    }
    expect(peakOf(0) <= 1.0f, "bounce 0 never goes past the end");
    float peak = peakOf(0.25f);
    expect(peak > 1.02f && peak < 1.04f, "bounce 0.25 goes about 3% past");
    expect(near(peak - 1, overshootOf(0.25f), 0.003f), "overshootOf says how far it goes past");
    expect(overshootOf(0) == 0, "no bounce, no overshoot");
    // Rises fast like the ease-out it replaced: most of the way in the first third.
    expect(springProgress(spring(0.25f), 0.4f) > 0.85f, "a spring covers most of the way early");
    expect(springProgress(spring(0.25f), 0.12f) < 0.5f, "...but not all of it at once: big morphs felt rushed at settle 8.5");
    Motion snappy = spring(0.25f);
    snappy.settle = 8.5f;
    expect(springProgress(snappy, 0.12f) > springProgress(spring(0.25f), 0.12f), "a higher settle is more front-loaded");
    expect(springProgress(spring(0), 0.5f) > springProgress(spring(0), 0.25f), "with no bounce it only moves forward");
}

void morphTests()
{
    Tokens t;
    Rect idle{ 190, 0, 160, 28 }, card{ 0, 0, 540, 420 };
    Motion grow = morphMotion(idle, card, t), shrink = morphMotion(card, idle, t);
    expect(grow.bounce == t.morphGrowBounce, "growing overshoots");
    expect(shrink.bounce == 0, "shrinking doesn't");
    Motion nudge = morphMotion(idle, { 186, 0, 168, 28 }, t);
    expect(nudge.duration > t.morphGrowShortest && nudge.duration < grow.duration, "a short way takes less");
    expect(shrink.duration < grow.duration, "shrinking is quicker than growing");
    // Square root through the reference: no edge where every big morph ends up the same.
    Rect wide{ 190 - 250, 0, 160 + 500, 28 }; // 500 wider, about the same centre: the reference distance
    expect(near(morphDistance(idle, wide), 500, 0.5f), "growing about its centre: only the size counts");
    expect(near(static_cast<float>(morphMotion(idle, wide, t).duration.count()), static_cast<float>(t.morphGrowReference.count()), 1),
           "the reference distance takes the reference time");
    Rect taller{ 0, 0, 540, 600 };
    expect(morphMotion(idle, taller, t).duration > grow.duration, "a taller card takes longer than a shorter one");
    expect(morphDistance(idle, { 190, 0, 160 + 300, 28 + 400 }) > morphDistance(idle, { 190, 0, 160, 28 + 400 }),
           "growing both ways counts more than one");
    expect(morphDistance(idle, { 300, 0, 160, 28 }) == 110, "sliding along the edge counts its move");
    Tokens huge;
    huge.morphReferenceDistance = 10;
    expect(morphMotion(idle, card, huge).duration == huge.morphGrowMax, "the ceiling is only a guard");
    expect(morphMotion(idle, idle, t).duration == t.morphGrowShortest, "no way at all, the shortest");
    expect(morphMotion(idle, { 300, 0, 160, 28 }, t).bounce == t.morphGrowBounce, "a move along the edge settles like growing");

    // Grown to the canvas's bottom: going past would be clipped.
    Rect tall{ 100, 0, 340, t.canvas.y - 2 };
    Motion fit = morphMotion(idle, tall, t);
    expect(fit.bounce < t.morphGrowBounce, "overshoot is trimmed near the canvas's bottom");
    expect(overshootOf(fit.bounce) * (tall.height - idle.height) <= 2.01f, "...to the room left");
    expect(morphMotion(idle, { 100, 0, 340, t.canvas.y }, t).bounce == 0, "no room, no overshoot");

    expect(towardSize({ Ms{ 300 }, 0.3f }, { 0, 0 }).bounce == 0, "tucking away to nothing never overshoots (no negative size)");
    expect(towardSize({ Ms{ 300 }, 0.3f }, { 20, 20 }).bounce == 0.3f, "...growing out of it may");

    Tokens reduced;
    reduced.reducedMotion = true;
    Motion calm = morphMotion(idle, card, reduced);
    expect(calm.bounce == 0 && calm.duration == reduced.reducedMotionDuration, "reduced motion: short, no overshoot");
}

void pulseTests()
{
    Tokens t;
    Scene compact, alert, next;
    compact.mode = Mode::Compact;
    compact.size = { 260, 32 };
    alert = compact;
    alert.mode = Mode::Alert;
    alert.alertPosted = Ms{ 1000 };
    next = alert;
    expect(alertPulses(compact, alert, t), "an alert in a pill that keeps its size pulses");
    expect(!alertPulses(alert, alert, t), "the same alert laid out again doesn't");
    next.alertPosted = Ms{ 2000 };
    expect(alertPulses(alert, next, t), "a new one (or the same one posted again) does");
    next.size = { 300, 40 };
    expect(!alertPulses(alert, next, t), "a morph shows the change itself");
    Tokens reduced;
    reduced.reducedMotion = true;
    expect(!alertPulses(compact, alert, reduced), "reduced motion: no pulse");
}

void revealTests()
{
    Tokens t;
    Rect idle{ 190, 0, 160, 28 }, card{ 0, 0, 540, 420 };
    Motion grow = morphMotion(idle, card, t);
    Ms reveal = revealDelay(grow, idle, card, t);
    float at = static_cast<float>(reveal.count()) / static_cast<float>(grow.duration.count());
    expect(reveal > Ms{ 0 } && reveal < grow.duration / 2, "growing: content comes in early in the morph");
    expect(springProgress(grow, at) >= t.contentRevealAt, "...once the pill is contentRevealAt of the way");
    expect(springProgress(grow, at - 0.02f) < t.contentRevealAt, "...and no later");
    Motion small = morphMotion(idle, { 180, 0, 180, 32 }, t);
    expect(revealDelay(small, idle, { 180, 0, 180, 32 }, t) < reveal, "a small change comes in sooner");
    expect(revealDelay(morphMotion(card, idle, t), card, idle, t) == Ms{ 0 }, "shrinking: at once");
    expect(revealDelay(grow, card, card, t) == Ms{ 0 }, "no morph: at once");
    Tokens reduced;
    reduced.reducedMotion = true;
    expect(revealDelay(grow, idle, card, reduced) == Ms{ 0 }, "reduced motion: at once");
    Scene was, next;
    next.size = { 400, 200 };
    SceneMotion m = sceneMotion(was, next, {}, t);
    expect(m.reveal == revealDelay(m.morph, interactiveBounds(was), interactiveBounds(next), t), "a scene change carries its reveal");
}

void staggerTests()
{
    Tokens t;
    std::vector<Ms> d = staggerDelays({ 40, 8, 8.5f, 70 }, t);
    expect(d[1] == Ms{ 0 } && d[2] == Ms{ 0 }, "the top row arrives first, side by side together");
    expect(d[0] == t.contentStagger && d[3] == t.contentStagger * 2, "each row after the one above");
    std::vector<float> many;
    for (int i = 0; i < 10; ++i) many.push_back(i * 20.0f);
    expect(staggerDelays(many, t).back() == t.contentStagger * static_cast<long long>(t.contentStaggerRows - 1), "rows past the limit arrive with the last");
    Tokens reduced;
    reduced.reducedMotion = true;
    auto none = staggerDelays({ 0, 40 }, reduced);
    expect(none[1] == Ms{ 0 }, "reduced motion: everything at once");
}

void sceneMotionTests()
{
    Tokens t;
    Scene lifted, landed;
    lifted.size = landed.size = { 260, 32 };
    lifted.lifted = true;
    lifted.offset = { 100, t.liftDrop };
    landed.offset = { 140, 0 };
    SceneMotion m = sceneMotion(lifted, landed, { 1.5f, 0.7f }, t);
    expect(m.fling == Vec2{ 1.5f, 0 }, "a lifted island let go carries its sideways speed into the landing");
    expect(m.morph == morphMotion(interactiveBounds(lifted), interactiveBounds(landed), t), "...along the morph to where it lands");
    expect(sceneMotion(landed, landed, { 1.5f, 0.7f }, t).fling == Vec2{}, "nothing let go, nothing flung");
    Scene dragged = landed, dropped = landed;
    dragged.bubble = BubbleScene{};
    dragged.bubble->dragCentre = Vec2{ 300, 40 };
    dropped.bubble = BubbleScene{};
    expect(sceneMotion(dragged, dropped, { 1.5f, 0.7f }, t).fling == Vec2{ 1.5f, 0.7f }, "the bubble let go carries it any way");
}

void flingTests()
{
    Tokens t;
    PointerTrack track;
    for (int i = 0; i <= 5; ++i) track = trackPointer(track, { i * 10.0f, 0 }, Ms{ 1000 + i * 10 }, t);
    Vec2 v = flingVelocity(track, Ms{ 1050 }, t);
    expect(near(v.x, 1.0f, 0.01f) && v.y == 0, "let go mid-drag: the pointer's speed");
    expect(flingVelocity(track, Ms{ 1050 } + t.flingStale + Ms{ 1 }, t) == Vec2{}, "stopped before letting go: no fling");
    PointerTrack fast;
    for (int i = 0; i <= 3; ++i) fast = trackPointer(fast, { i * 100.0f, 0 }, Ms{ i * 10 }, t);
    expect(near(flingVelocity(fast, Ms{ 30 }, t).x, t.flingMaxSpeed), "capped at flingMaxSpeed");
    PointerTrack same = trackPointer(trackPointer(PointerTrack{}, { 0, 0 }, Ms{ 0 }, t), { 50, 0 }, Ms{ 0 }, t);
    expect(same.velocity == Vec2{} && same.at == Vec2{}, "two moves in one millisecond wait for the next");
    Tokens reduced;
    reduced.reducedMotion = true;
    expect(flingVelocity(track, Ms{ 1050 }, reduced) == Vec2{}, "reduced motion: no fling");
}

} // namespace

void runMotionTests()
{
    springTests();
    morphTests();
    pulseTests();
    revealTests();
    staggerTests();
    sceneMotionTests();
    flingTests();
}

} // namespace cup::test
