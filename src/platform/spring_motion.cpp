#include "platform/spring_motion.h"

#include <algorithm>
#include <string>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Composition rejects zero-length animations.
Ms atLeast1(Ms d) { return std::max(d, Ms{ 1 }); }

float stepAt(int i) { return static_cast<float>(i) / kSpringKeyframes; }

// Size (Vector2) or Offset (Vector3), from where it is to what's set. Straight
// lines between the samples: the curve is in the samples, not the easing.
KeyFrameAnimation along(KeyFrameAnimation const& animation, const wchar_t* target, const Motion& motion, LinearEasingFunction const& linear)
{
    for (int i = 1; i < kSpringKeyframes; ++i) {
        float p = springProgress(motion.bounce, stepAt(i));
        animation.InsertExpressionKeyFrame(stepAt(i), L"this.StartingValue + (this.FinalValue - this.StartingValue) * " + std::to_wstring(p), linear);
    }
    animation.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", linear);
    animation.Duration(atLeast1(motion.duration));
    animation.Target(target);
    return animation;
}

} // namespace

ImplicitAnimationCollection springMorph(Compositor const& c, const Motion& motion)
{
    // this.StartingValue is where the property is when it's set (mid-morph:
    // where that morph had got to), the same place the interaction layer
    // picks up from (decision 001).
    auto linear = c.CreateLinearEasingFunction();
    auto collection = c.CreateImplicitAnimationCollection();
    collection.Insert(L"Size", along(c.CreateVector2KeyFrameAnimation(), L"Size", motion, linear));
    collection.Insert(L"Offset", along(c.CreateVector3KeyFrameAnimation(), L"Offset", motion, linear));
    return collection;
}

Vector3KeyFrameAnimation springOffset(Compositor const& c, Vec2 from, Vec2 to, const Motion& motion, Vec2 fling)
{
    // The two parts of a spring add up: the way home from `from`, plus the
    // push of a throw from nothing (core springCarry, in durations: px/ms * ms).
    float reach = static_cast<float>(motion.duration.count());
    auto anim = c.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(0.0f, toFloat3(from));
    auto linear = c.CreateLinearEasingFunction();
    for (int i = 1; i <= kSpringKeyframes; ++i) {
        float t = stepAt(i), home = springProgress(motion.bounce, t), push = springCarry(motion.bounce, t) * reach;
        anim.InsertKeyFrame(t, toFloat3({ from.x + (to.x - from.x) * home + fling.x * push, from.y + (to.y - from.y) * home + fling.y * push }), linear);
    }
    anim.Duration(atLeast1(motion.duration));
    return anim;
}

Vector3KeyFrameAnimation pulseScale(Compositor const& c, float peak, const Motion& motion)
{
    // Up quickly (the first fifth, easing out like a morph), then the spring brings it back.
    constexpr float kSwell = 0.2f;
    const Bezier& rise = tokens().morphEase;
    auto anim = c.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(0.0f, { 1.0f, 1.0f, 1.0f });
    anim.InsertKeyFrame(kSwell, { peak, peak, 1.0f }, c.CreateCubicBezierEasingFunction(toFloat2(rise.c1), toFloat2(rise.c2)));
    auto linear = c.CreateLinearEasingFunction();
    for (int i = 1; i <= kSpringKeyframes; ++i) {
        float back = springProgress(motion.bounce, stepAt(i)), scale = peak + (1.0f - peak) * back;
        anim.InsertKeyFrame(kSwell + (1.0f - kSwell) * stepAt(i), { scale, scale, 1.0f }, linear);
    }
    anim.Duration(atLeast1(motion.duration));
    return anim;
}

} // namespace cup::platform
