#include "platform/daydream.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

constexpr float kPi = 3.14159265f;
constexpr float kTurn = 2 * kPi;

// Expression building blocks: `p` is the pill's body, `f` the property set.
const std::wstring kCx = L"(p.Offset.X + p.Size.X / 2)";
const std::wstring kCy = L"(p.Offset.Y + p.Size.Y / 2)";
// How far down it floats: enough that, turned any way, its far corners stay
// daydreamClearance below the screen edge (the render layer ends there).
const std::wstring kLift = L"Max(0, Length(p.Size) / 2 + f.Clearance - " + kCy + L")";
// The jelly squash: in pill axes (Scale applies before RotationAngle), out
// one way while in the other. Whole multiples of Phase, so a loop wraps seamlessly.
const std::wstring kWobble = L"(f.Float * f.Wobble * (0.7 * Sin(5 * f.Phase) + 0.3 * Sin(3 * f.Phase)))";
// The droplet: its size, and how far out of the pill's edge its centre is.
// From -size/2 (half inside: merged, the neck at its thickest) to past
// bridgeReach (the neck snapped), three times a turn.
const std::wstring kD = L"(f.Droplet * f.Float)";
const std::wstring kOut = L"(-f.Droplet / 2 + (1 - Cos(3 * f.Phase)) / 2 * (f.Droplet + 2.5 * f.Reach))";
const std::wstring kAngle = L"(-2 * f.Phase)"; // against the spin: it visibly travels round

void bind(Compositor const& c, CompositionObject const& target, wchar_t const* property, std::wstring const& expression,
          Visual const& pill, CompositionPropertySet const& params)
{
    auto anim = c.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"p", pill);
    anim.SetReferenceParameter(L"f", params);
    target.StartAnimation(property, anim);
}

Ms ms(float value) { return Ms{ std::max(1LL, static_cast<long long>(value)) }; }

} // namespace

void Daydream::build(wuc::Compositor const& c, wuc::ContainerVisual const& island, wuc::Visual const& pill)
{
    compositor_ = c;
    params_ = c.CreatePropertySet();
    params_.InsertScalar(L"Float", 0.0f);
    params_.InsertScalar(L"Phase", 0.0f);
    brush_ = c.CreateColorBrush();
    refreshAnimations();

    // At rest (Float 0) every one of these is the identity: the island hangs
    // where the scene puts it, and the droplet has no size.
    bind(c, island, L"CenterPoint", L"Vector3(" + kCx + L", " + kCy + L", 0)", pill, params_);
    bind(c, island, L"Offset",
         L"Vector3(f.Float * f.DriftX * Sin(f.Phase), f.Float * (" + kLift + L" + f.DriftY * (1 - Cos(2 * f.Phase)) / 2), 0)", pill, params_);
    bind(c, island, L"RotationAngle", L"f.Phase", pill, params_);
    bind(c, island, L"Scale", L"Vector3(1 + " + kWobble + L", 1 - " + kWobble + L", 1)", pill, params_);

    droplet_ = c.CreateShapeVisual();
    auto round = c.CreateRoundedRectangleGeometry();
    bind(c, round, L"Size", L"Vector2(" + kD + L", " + kD + L")", pill, params_);
    bind(c, round, L"CornerRadius", L"Vector2(" + kD + L" / 2, " + kD + L" / 2)", pill, params_);
    auto fill = c.CreateSpriteShape(round);
    fill.FillBrush(brush_);
    droplet_.Shapes().Append(fill);
    bind(c, droplet_, L"Size", L"Vector2(" + kD + L", " + kD + L")", pill, params_);
    bind(c, droplet_, L"Offset",
         L"Vector3(" + kCx + L" + Cos(" + kAngle + L") * (p.Size.X / 2 + " + kOut + L") - " + kD + L" / 2, " + kCy + L" + Sin(" + kAngle +
             L") * (p.Size.Y / 2 + " + kOut + L") - " + kD + L" / 2, 0)",
         pill, params_);
    // Under the pill, so while merged it's hidden inside it; the neck under both.
    island.Children().InsertAtBottom(droplet_);
    neck_.build(c, island, pill, droplet_);
}

void Daydream::refreshAnimations()
{
    const Tokens& t = tokens();
    params_.InsertScalar(L"DriftX", t.daydreamDrift.x);
    params_.InsertScalar(L"DriftY", t.daydreamDrift.y);
    params_.InsertScalar(L"Clearance", t.daydreamClearance);
    params_.InsertScalar(L"Wobble", t.daydreamWobble);
    params_.InsertScalar(L"Droplet", t.daydreamDropletSize);
    params_.InsertScalar(L"Reach", std::max(t.bridgeReach, 0.01f));
    brush_.Color(toUiColor(t.islandColor)); // the island's colour: it reads as part of it
    if (droplet_) neck_.refreshAnimations();
}

Ms Daydream::set(bool on)
{
    if (on == on_) return Ms{ 0 };
    on_ = on;
    if (on) {
        start();
        return Ms{ 0 };
    }
    return stop();
}

void Daydream::start()
{
    int run = ++run_;
    // Landing still: carry on from the turn it was settling on, forwards.
    base_ = stage_ == Stage::Landing ? landTo_ : 0.0f;
    const Tokens& t = tokens();
    // Keyframes without a start: from wherever the value is now, even mid-landing.
    auto rise = compositor_.CreateScalarKeyFrameAnimation();
    rise.InsertKeyFrame(1.0f, 1.0f, easingFor(compositor_, t.morphEase));
    rise.Duration(std::max(t.daydreamRise, Ms{ 1 }));

    // Spin up: half a turn, slow at first, ending at the loop's speed. This
    // curve's end slope is 2, and half a turn over a loop's time at twice its
    // average speed is a whole turn per loop: no jolt where the loop takes over.
    auto spinUp = compositor_.CreateScalarKeyFrameAnimation();
    spinUp.InsertKeyFrame(1.0f, base_ + kPi, easingFor(compositor_, Bezier{ { 0.4f, 0.0f }, { 0.8f, 0.6f } }));
    spinUp.Duration(std::max(t.daydreamLoop, Ms{ 1 }));

    auto batch = compositor_.CreateScopedBatch(CompositionBatchTypes::Animation);
    params_.StartAnimation(L"Float", rise);
    params_.StartAnimation(L"Phase", spinUp);
    batch.End();
    batch.Completed([this, run](wf::IInspectable const&, CompositionBatchCompletedEventArgs const&) {
        if (run == run_) spin();
    });
    stage_ = Stage::SpinUp;
    since_ = Clock::now();
}

void Daydream::spin()
{
    // Whole turns forever, at a steady speed: everything in Phase repeats.
    auto loop = compositor_.CreateScalarKeyFrameAnimation();
    loop.InsertKeyFrame(0.0f, base_ + kPi);
    loop.InsertKeyFrame(1.0f, base_ + kPi + kTurn, compositor_.CreateLinearEasingFunction());
    loop.Duration(std::max(tokens().daydreamLoop, Ms{ 1 }));
    loop.IterationBehavior(AnimationIterationBehavior::Forever);
    params_.StartAnimation(L"Phase", loop);
    stage_ = Stage::Looping;
    since_ = Clock::now();
}

float Daydream::phaseNow() const
{
    float loop = static_cast<float>(std::max(tokens().daydreamLoop, Ms{ 1 }).count());
    float elapsed = std::chrono::duration<float, std::milli>(Clock::now() - since_).count();
    switch (stage_) {
    case Stage::SpinUp: return base_ + kPi * std::min(elapsed / loop, 1.0f) * std::min(elapsed / loop, 1.0f); // about: it eases in
    case Stage::Looping: return base_ + kPi + kTurn * std::fmod(elapsed / loop, 1.0f);
    case Stage::Landing: return landTo_;
    case Stage::Rest: break;
    }
    return 0;
}

Ms Daydream::stop()
{
    int run = ++run_;
    const Tokens& t = tokens();
    // Finish the turn it's in, forwards, slowing to a stop exactly upright
    // (a whole turn): never unwinding. The margin keeps a turn that's almost
    // done, by the app's reckoning, from snapping back if it's actually just past.
    float now = phaseNow();
    landTo_ = kTurn * std::ceil((now + 0.3f) / kTurn);
    // An ease-out starts at twice its average speed: at the loop's speed, the
    // landing takes twice as long as the rest of the turn would at that speed.
    float loop = static_cast<float>(std::max(t.daydreamLoop, Ms{ 1 }).count());
    float rest = (landTo_ - now) / kTurn * loop * 2;
    Ms duration = ms(std::clamp(rest, static_cast<float>(t.daydreamSettle.count()), std::max(2 * loop, static_cast<float>(t.daydreamSettle.count()))));

    auto turn = compositor_.CreateScalarKeyFrameAnimation();
    turn.InsertKeyFrame(1.0f, landTo_, easingFor(compositor_, Bezier{ { 0.3f, 0.6f }, { 0.5f, 1.0f } })); // starts at slope 2, see above
    turn.Duration(duration);
    // Down only as the turn finishes: it doesn't touch the edge still turned.
    auto down = compositor_.CreateScalarKeyFrameAnimation();
    down.InsertKeyFrame(1.0f, 0.0f, easingFor(compositor_, Bezier{ { 0.6f, 0.0f }, { 0.4f, 1.0f } }));
    down.Duration(duration);

    auto batch = compositor_.CreateScopedBatch(CompositionBatchTypes::Animation);
    params_.StartAnimation(L"Phase", turn); // replaces the endless loop
    params_.StartAnimation(L"Float", down);
    batch.End();
    batch.Completed([this, run](wf::IInspectable const&, CompositionBatchCompletedEventArgs const&) {
        if (run != run_) return; // held again meanwhile
        params_.InsertScalar(L"Phase", 0.0f); // the same as a whole turn: nothing moves
        stage_ = Stage::Rest;
    });
    stage_ = Stage::Landing;
    since_ = Clock::now();
    return duration;
}

} // namespace cup::platform
