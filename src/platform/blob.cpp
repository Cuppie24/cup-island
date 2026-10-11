#include "platform/blob.h"

#include "core/tokens.h"

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Binds a property to an expression over the body's live (animating) values,
// so it follows every morph and drag without app code running per frame.
void bind(Compositor const& c, CompositionObject const& target, wchar_t const* property, wchar_t const* expression,
          ShapeVisual const& body, CompositionPropertySet const& shape, CompositionPropertySet const& drag)
{
    auto anim = c.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"body", body);
    anim.SetReferenceParameter(L"shape", shape);
    anim.SetReferenceParameter(L"drag", drag);
    target.StartAnimation(property, anim);
}

CubicBezierEasingFunction easingFor(Compositor const& c, Bezier curve)
{
    return c.CreateCubicBezierEasingFunction(toFloat2(curve.c1), toFloat2(curve.c2));
}

ImplicitAnimationCollection morphAnimations(Compositor const& c)
{
    auto easing = easingFor(c, tokens().morphEase);

    auto size = c.CreateVector2KeyFrameAnimation();
    size.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easing);
    size.Duration(tokens().morphDuration);
    size.Target(L"Size");

    auto offset = c.CreateVector3KeyFrameAnimation();
    offset.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easing);
    offset.Duration(tokens().morphDuration);
    offset.Target(L"Offset");

    auto collection = c.CreateImplicitAnimationCollection();
    collection.Insert(L"Size", size);
    collection.Insert(L"Offset", offset);
    return collection;
}

// Corner radius: half the shorter side, capped so big shapes aren't blobs.
constexpr wchar_t kRadius[] = L"Min(Min(body.Size.X, body.Size.Y) / 2, shape.MaxRadius)";

} // namespace

void Blob::build(wuc::Compositor const& c, GraphicsDevice& device, wuc::ContainerVisual const& bodies,
                 wuc::ContainerVisual const& contents, Vec2 size, Vec2 offset, bool round)
{
    compositor_ = c;
    size_ = size;
    offset_ = offset;
    round_ = round;

    body_ = c.CreateShapeVisual();
    body_.Size(toFloat2(size));
    body_.Offset(toFloat3(offset));
    bodies.Children().InsertAtTop(body_);

    shape_ = c.CreatePropertySet();
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
    shape_.InsertScalar(L"Round", round ? 1.0f : 0.0f);
    drag_ = c.CreatePropertySet();
    drag_.InsertVector2(L"Centre", { offset.x + size.x / 2, offset.y + size.y / 2 });

    // Press/lift scale pivots on the top edge's middle for the pill (it stays
    // glued to the screen edge), on the centre for the bubble.
    bind(c, body_, L"CenterPoint", L"Vector3(body.Size.X / 2, body.Size.Y / 2 * shape.Round, 0)", body_, shape_, drag_);

    // Background. As a pill only the bottom corners are rounded (a notch
    // hanging from the screen edge): the geometry is made a radius taller and
    // shifted up, and ShapeVisual clips the top corners away. Round = 1 drops
    // the shift, giving a full circle; in between it's a smooth blend.
    std::wstring r = kRadius;
    auto background = c.CreateRoundedRectangleGeometry();
    bind(c, background, L"CornerRadius", (L"Vector2(" + r + L", " + r + L")").c_str(), body_, shape_, drag_);
    bind(c, background, L"Offset", (L"Vector2(0, -" + r + L" * (1 - shape.Round))").c_str(), body_, shape_, drag_);
    bind(c, background, L"Size", (L"Vector2(body.Size.X, body.Size.Y + " + r + L" * (1 - shape.Round))").c_str(), body_, shape_, drag_);
    auto backgroundShape = c.CreateSpriteShape(background);
    backgroundShape.FillBrush(c.CreateColorBrush({ 255, 0, 0, 0 }));
    body_.Shapes().Append(backgroundShape);

    // Content: in the shared content layer, tracking the body's live place,
    // size and scale, clipped to it so content fading in during a grow morph
    // never shows outside the shape.
    auto content = c.CreateContainerVisual();
    bind(c, content, L"Offset", L"body.Offset", body_, shape_, drag_);
    bind(c, content, L"Size", L"body.Size", body_, shape_, drag_);
    bind(c, content, L"CenterPoint", L"body.CenterPoint", body_, shape_, drag_);
    bind(c, content, L"Scale", L"body.Scale", body_, shape_, drag_);
    content.Clip(c.CreateInsetClip());
    contents.Children().InsertAtTop(content);
    elements_.init(c, device, content);

    // Attached last, so the initial Size/Offset above don't animate.
    refreshAnimations();
}

void Blob::refreshAnimations()
{
    body_.ImplicitAnimations(morphAnimations(compositor_));
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
}

void Blob::place(Vec2 size, Vec2 offset)
{
    // Only touch geometry on a real change: re-setting a property restarts
    // its implicit animation, which would stall a morph in flight. While
    // following, Offset belongs to the cursor expression; it lands later.
    if (size != size_) body_.Size(toFloat2(size));
    if (offset != offset_ && !following_) body_.Offset(toFloat3(offset));
    size_ = size;
    offset_ = offset;
}

void Blob::setRound(bool round)
{
    if (round == round_) return;
    round_ = round;
    auto anim = compositor_.CreateScalarKeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, round ? 1.0f : 0.0f, easingFor(compositor_, tokens().morphEase));
    anim.Duration(tokens().morphDuration);
    shape_.StartAnimation(L"Round", anim);
}

void Blob::follow(std::optional<Vec2> centre)
{
    const Tokens& t = tokens();
    if (centre) {
        drag_.InsertVector2(L"Centre", toFloat2(*centre));
        if (following_) return;
        // Centred on the cursor from the live size, so growing into the pill
        // (or shrinking back) during the drag never moves it off the cursor.
        following_ = true;
        auto anim = compositor_.CreateExpressionAnimation(L"Vector3(drag.Centre.X - body.Size.X / 2, drag.Centre.Y - body.Size.Y / 2, 0)");
        anim.SetReferenceParameter(L"body", body_);
        anim.SetReferenceParameter(L"drag", drag_);
        body_.StartAnimation(L"Offset", anim);
        playScale(t.dragLiftScale, t.pressReleaseDuration, t.pressReleaseEase); // picked up
        return;
    }
    if (!following_) return;
    // Let go: stopping keeps the current (cursor) position, then the implicit
    // morph carries it to where it lands (the pill, or back home).
    following_ = false;
    body_.StopAnimation(L"Offset");
    body_.Offset(toFloat3(offset_));
    playScale(1.0f, t.pressReleaseDuration, t.pressReleaseEase);
}

void Blob::pressDown()
{
    const Tokens& t = tokens();
    playScale(t.pressScale, t.pressDownDuration, t.pressDownEase);
}

void Blob::pressRelease()
{
    const Tokens& t = tokens();
    playScale(1.0f, t.pressReleaseDuration, t.pressReleaseEase);
}

void Blob::playScale(float scale, Ms duration, Bezier curve)
{
    auto anim = compositor_.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { scale, scale, 1.0f }, easingFor(compositor_, curve));
    anim.Duration(duration);
    body_.StartAnimation(L"Scale", anim); // replaces whatever was animating Scale
}

} // namespace cup::platform
