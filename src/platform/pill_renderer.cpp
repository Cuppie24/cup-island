#include "platform/pill_renderer.h"

#include "core/tokens.h"

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;
using winrt::Windows::UI::Color;

// Binds a property to an expression over the pill's live (animating) size,
// so it follows every morph without app code running per frame.
void bindToPill(Compositor const& c, CompositionObject const& target, wchar_t const* property,
                wchar_t const* expression, ShapeVisual const& pill)
{
    auto anim = c.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"pill", pill);
    target.StartAnimation(property, anim);
}

CompositionSpriteShape roundedBar(Compositor const& c, ShapeVisual const& pill, Vec2 size, float radius,
                                  wchar_t const* offsetExpression, Color color)
{
    auto geometry = c.CreateRoundedRectangleGeometry();
    geometry.CornerRadius({ radius, radius });
    geometry.Size(toFloat2(size));
    bindToPill(c, geometry, L"Offset", offsetExpression, pill);
    auto shape = c.CreateSpriteShape(geometry);
    shape.FillBrush(c.CreateColorBrush(color));
    return shape;
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

// Content fades slightly after the pill starts morphing: the container moves
// first, then content crossfades, like the real Dynamic Island.
ImplicitAnimationCollection fadeAnimations(Compositor const& c)
{
    auto easing = easingFor(c, tokens().morphEase);

    auto opacity = c.CreateScalarKeyFrameAnimation();
    opacity.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easing);
    opacity.Duration(tokens().detailFadeDuration);
    opacity.DelayTime(tokens().detailFadeDelay);
    opacity.DelayBehavior(AnimationDelayBehavior::SetInitialValueBeforeDelay);
    opacity.Target(L"Opacity");

    auto collection = c.CreateImplicitAnimationCollection();
    collection.Insert(L"Opacity", opacity);
    return collection;
}

void playScale(Compositor const& c, ShapeVisual const& pill, float scale, Ms duration, Bezier curve)
{
    auto easing = easingFor(c, curve);
    auto anim = c.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { scale, scale, 1.0f }, easing);
    anim.Duration(duration);
    pill.StartAnimation(L"Scale", anim); // replaces whatever was animating Scale
}

} // namespace

void PillRenderer::build(wuc::Compositor const& c, wuc::ContainerVisual const& root, const Scene& initial)
{
    compositor_ = c;

    pill_ = c.CreateShapeVisual();
    pill_.Size(toFloat2(initial.size));
    pill_.Offset(toFloat3(initial.offset));

    // Press-scale pivots on the middle of the top edge, so the pill stays glued
    // to the screen edge while its sides and bottom squash in.
    bindToPill(c, pill_, L"CenterPoint", L"Vector3(pill.Size.X / 2, 0, 0)", pill_);

    // Background: only the bottom corners are rounded (a notch hanging from the
    // screen edge). The geometry has one radius for all corners, so it's made
    // a radius taller and shifted up; ShapeVisual clips the top corners away.
    auto background = c.CreateRoundedRectangleGeometry();
    bindToPill(c, background, L"CornerRadius",
        L"Vector2(Min(pill.Size.X, pill.Size.Y) / 2, Min(pill.Size.X, pill.Size.Y) / 2)", pill_);
    bindToPill(c, background, L"Offset", L"Vector2(0, -Min(pill.Size.X, pill.Size.Y) / 2)", pill_);
    bindToPill(c, background, L"Size", L"Vector2(pill.Size.X, pill.Size.Y + Min(pill.Size.X, pill.Size.Y) / 2)", pill_);
    auto backgroundShape = c.CreateSpriteShape(background);
    backgroundShape.FillBrush(c.CreateColorBrush({ 255, 0, 0, 0 }));
    pill_.Shapes().Append(backgroundShape);

    // Detail: placeholder layout (art + title/subtitle bars + progress track)
    // until the PS §5 content model is rendered. Positions follow the pill's size.
    detail_ = c.CreateContainerVisual();
    detail_.Opacity(initial.detailOpacity);

    auto art = c.CreateEllipseGeometry();
    art.Radius({ 16.0f, 16.0f });
    bindToPill(c, art, L"Center", L"Vector2(30, pill.Size.Y / 2)", pill_);
    auto artShape = c.CreateSpriteShape(art);
    artShape.FillBrush(c.CreateColorBrush({ 255, 90, 200, 170 }));

    auto detailShapes = c.CreateShapeVisual();
    bindToPill(c, detailShapes, L"Size", L"pill.Size", pill_);
    detailShapes.Shapes().Append(artShape);
    detailShapes.Shapes().Append(roundedBar(c, pill_, { 180, 14 }, 5, L"Vector2(58, pill.Size.Y / 2 - 18)", { 230, 255, 255, 255 }));
    detailShapes.Shapes().Append(roundedBar(c, pill_, { 120, 10 }, 4, L"Vector2(58, pill.Size.Y / 2 + 4)", { 140, 255, 255, 255 }));
    detailShapes.Shapes().Append(roundedBar(c, pill_, { 300, 4 }, 2, L"Vector2(58, pill.Size.Y - 20)", { 60, 255, 255, 255 }));
    detailShapes.Shapes().Append(roundedBar(c, pill_, { 120, 4 }, 2, L"Vector2(58, pill.Size.Y - 20)", { 210, 255, 255, 255 }));

    detail_.Children().InsertAtTop(detailShapes);
    pill_.Children().InsertAtTop(detail_);
    root.Children().InsertAtTop(pill_);

    // Attached last, so the initial Size/Offset/Opacity above don't animate.
    refreshAnimations();
}

void PillRenderer::refreshAnimations()
{
    pill_.ImplicitAnimations(morphAnimations(compositor_));
    detail_.ImplicitAnimations(fadeAnimations(compositor_));
}

void PillRenderer::apply(const Scene& scene)
{
    pill_.Size(toFloat2(scene.size));
    pill_.Offset(toFloat3(scene.offset));
    detail_.Opacity(scene.detailOpacity);
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
