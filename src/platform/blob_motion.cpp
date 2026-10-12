// Blob's motion off the screen edge and back: the dragged bubble, the lifted
// island (decision 026), and their flared corners folding (decision 025).
// Composition hand-offs here follow decision 008.

#include "platform/blob.h"

#include "platform/blob_shape.h"

#include <algorithm>

namespace cup::platform {

void Blob::follow(std::optional<Vec2> centre)
{
    if (!centre) return land();
    centre_ = *centre;
    lifted_ = false;
    drag_.InsertVector2(L"Centre", toFloat2(*centre));
    if (!following_) startFollowing(L"Vector3(drag.Centre.X - pill.Size.X / 2, drag.Centre.Y - pill.Size.Y / 2, 0)");
}

void Blob::lift(std::optional<float> centreX)
{
    if (!centreX) return land();
    centre_ = { *centreX, 0 };
    lifted_ = true;
    drag_.InsertVector2(L"Centre", { *centreX, 0 });
    if (following_) return;
    // Its top stays liftDrop below the edge while its size morphs (an alert
    // arriving), and it sinks there instead of jumping.
    drag_.InsertScalar(L"Drop", 0.0f);
    auto sink = compositor_.CreateScalarKeyFrameAnimation();
    sink.InsertKeyFrame(1.0f, tokens().liftDrop, easingFor(compositor_, tokens().morphEase));
    sink.Duration(tokens().morphDuration);
    drag_.StartAnimation(L"Drop", sink);
    startFollowing(L"Vector3(drag.Centre.X - pill.Size.X / 2, drag.Drop, 0)");
}

void Blob::startFollowing(std::wstring const& expression)
{
    following_ = true;
    auto anim = compositor_.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"pill", body_);
    anim.SetReferenceParameter(L"drag", drag_);
    body_.StartAnimation(L"Offset", anim);
    setAttached(false); // off the screen edge: round, no ears (decision 025)
}

void Blob::land()
{
    if (!following_) return;
    // Let go: glide from where it was dropped to where it belongs. An explicit
    // animation with both ends, because stopping the cursor expression and
    // setting Offset left it stuck at the drop point on a real build (decision 008).
    following_ = false;
    Vec2 from = lifted_ ? Vec2{ centre_.x - size_.x / 2, tokens().liftDrop } : Vec2{ centre_.x - size_.x / 2, centre_.y - size_.y / 2 };
    body_.ImplicitAnimations(nullptr); // so setting the resting value below doesn't start a morph of its own
    body_.Offset(toFloat3(offset_));
    body_.ImplicitAnimations(implicit_);
    auto home = compositor_.CreateVector3KeyFrameAnimation();
    home.InsertKeyFrame(0.0f, toFloat3(from));
    home.InsertKeyFrame(1.0f, toFloat3(offset_), easingFor(compositor_, tokens().morphEase));
    home.Duration(tokens().morphDuration);
    body_.StartAnimation(L"Offset", home); // replaces the cursor expression
    setAttached(true);
}

void Blob::setAttached(bool on)
{
    // One after the other, never together: the ears sit on the top corners,
    // so rounding those while an ear is still there leaves a sliver of it
    // floating off the shape. Leaving the edge, the ears draw back into the
    // body first, then the corners round; landing, the reverse. The second
    // waits out the first (a delay holds the current value meanwhile).
    Ms whole = tokens().morphDuration, first = whole * 2 / 5;
    auto play = [&](const wchar_t* property, float to, Ms delay, Ms duration) {
        auto anim = compositor_.CreateScalarKeyFrameAnimation();
        anim.InsertKeyFrame(1.0f, to, easingFor(compositor_, tokens().morphEase));
        anim.DelayTime(delay);
        anim.Duration(std::max(duration, Ms{ 1 }));
        shape_.StartAnimation(property, anim);
    };
    play(L"Flare", on ? 1.0f : 0.0f, on ? whole - first : Ms{ 0 }, first);
    play(L"Attached", on ? 1.0f : 0.0f, on ? Ms{ 0 } : first, whole - first);
}

void Blob::setEars(bool left, bool right)
{
    // Flush with the screen's edge, that side's ear would hang off the
    // monitor: it folds into the body there (decision 026).
    auto fold = [&](const wchar_t* property, bool on, bool& current) {
        if (on == current) return;
        current = on;
        auto anim = compositor_.CreateScalarKeyFrameAnimation();
        anim.InsertKeyFrame(1.0f, on ? 1.0f : 0.0f, easingFor(compositor_, tokens().morphEase));
        anim.Duration(tokens().morphDuration);
        shape_.StartAnimation(property, anim);
    };
    fold(L"LeftEar", left, leftEar_);
    fold(L"RightEar", right, rightEar_);
}

} // namespace cup::platform
