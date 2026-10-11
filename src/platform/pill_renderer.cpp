#include "platform/pill_renderer.h"

#include "core/tokens.h"

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Binds a property to an expression over the pill's live (animating) size,
// so it follows every morph without app code running per frame.
void bindToPill(Compositor const& c, CompositionObject const& target, wchar_t const* property,
                wchar_t const* expression, ShapeVisual const& pill, CompositionPropertySet const& shape = nullptr)
{
    auto anim = c.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"pill", pill);
    if (shape) anim.SetReferenceParameter(L"shape", shape);
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

void playScale(Compositor const& c, ShapeVisual const& pill, float scale, Ms duration, Bezier curve)
{
    auto anim = c.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { scale, scale, 1.0f }, easingFor(c, curve));
    anim.Duration(duration);
    pill.StartAnimation(L"Scale", anim); // replaces whatever was animating Scale
}

} // namespace

void PillRenderer::build(wuc::Compositor const& c, wuc::ContainerVisual const& root, const Scene& initial)
{
    compositor_ = c;
    device_.create(c);
    size_ = initial.size;
    offset_ = initial.offset;

    pill_ = c.CreateShapeVisual();
    pill_.Size(toFloat2(initial.size));
    pill_.Offset(toFloat3(initial.offset));

    // Press-scale pivots on the middle of the top edge, so the pill stays glued
    // to the screen edge while its sides and bottom squash in.
    bindToPill(c, pill_, L"CenterPoint", L"Vector3(pill.Size.X / 2, 0, 0)", pill_);

    // Background: only the bottom corners are rounded (a notch hanging from the
    // screen edge). The geometry has one radius for all corners, so it's made
    // a radius taller and shifted up; ShapeVisual clips the top corners away.
    // Radius = half the height, capped by maxCornerRadius.
    shape_ = c.CreatePropertySet();
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
    auto background = c.CreateRoundedRectangleGeometry();
    bindToPill(c, background, L"CornerRadius",
        L"Vector2(Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius), Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))",
        pill_, shape_);
    bindToPill(c, background, L"Offset", L"Vector2(0, -Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", pill_, shape_);
    bindToPill(c, background, L"Size",
        L"Vector2(pill.Size.X, pill.Size.Y + Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", pill_, shape_);
    auto backgroundShape = c.CreateSpriteShape(background);
    backgroundShape.FillBrush(c.CreateColorBrush({ 255, 0, 0, 0 }));
    pill_.Shapes().Append(backgroundShape);

    // Content lives in a container clipped to the pill's live size, so new
    // content fading in during a grow morph never shows outside the pill.
    auto content = c.CreateContainerVisual();
    bindToPill(c, content, L"Size", L"pill.Size", pill_);
    content.Clip(c.CreateInsetClip());
    pill_.Children().InsertAtTop(content);
    elements_.init(c, device_, content);
    elements_.apply(initial);

    root.Children().InsertAtTop(pill_);

    // Attached last, so the initial Size/Offset above don't animate.
    refreshAnimations();
}

void PillRenderer::refreshAnimations()
{
    pill_.ImplicitAnimations(morphAnimations(compositor_));
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
}

bool PillRenderer::apply(const Scene& scene)
{
    // Only touch geometry on a real change: re-setting a property restarts
    // its implicit animation, which would stall a morph in flight.
    bool geometryChanged = scene.size != size_ || scene.offset != offset_;
    if (geometryChanged) {
        size_ = scene.size;
        offset_ = scene.offset;
        pill_.Size(toFloat2(scene.size));
        pill_.Offset(toFloat3(scene.offset));
    }
    elements_.apply(scene);
    return geometryChanged;
}

void PillRenderer::pressDown()
{
    const Tokens& t = tokens();
    playScale(compositor_, pill_, t.pressScale, t.pressDownDuration, t.pressDownEase);
}

void PillRenderer::pressRelease()
{
    const Tokens& t = tokens();
    playScale(compositor_, pill_, 1.0f, t.pressReleaseDuration, t.pressReleaseEase);
}

} // namespace cup::platform
