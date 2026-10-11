#include "platform/blob.h"

#include "platform/element_parts.h"

#include <algorithm>
#include <utility>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Binds a property to an expression over the body's live (animating) size,
// so it follows every morph without app code running per frame.
void bind(Compositor const& c, CompositionObject const& target, wchar_t const* property, wchar_t const* expression,
          ShapeVisual const& body, CompositionPropertySet const& shape)
{
    auto anim = c.CreateExpressionAnimation(expression);
    anim.SetReferenceParameter(L"pill", body);
    anim.SetReferenceParameter(L"shape", shape);
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

winrt::Windows::UI::Color fillColor(bool highlighted)
{
    return toUiColor(highlighted ? tokens().islandHighlightColor : tokens().islandColor);
}

} // namespace

void Blob::build(wuc::Compositor const& c, GraphicsDevice& device, wuc::ContainerVisual const& parent, Vec2 size, Vec2 offset)
{
    compositor_ = c;
    size_ = size;
    offset_ = offset;

    body_ = c.CreateShapeVisual();
    body_.Size(toFloat2(size));
    body_.Offset(toFloat3(offset));
    drag_ = c.CreatePropertySet();
    drag_.InsertVector2(L"Centre", { offset.x + size.x / 2, offset.y + size.y / 2 });

    // Press-scale pivots on the middle of the top edge, so the shape stays
    // glued to the screen edge while its sides and bottom squash in.
    shape_ = c.CreatePropertySet();
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
    bind(c, body_, L"CenterPoint", L"Vector3(pill.Size.X / 2, 0, 0)", body_, shape_);

    // Background: only the bottom corners are rounded (a notch hanging from the
    // screen edge). The geometry has one radius for all corners, so it's made
    // a radius taller and shifted up; ShapeVisual clips the top corners away.
    // Radius = half the height, capped by maxCornerRadius: the same curve as
    // core pillCornerRadius(), which content radii are nested in.
    auto background = c.CreateRoundedRectangleGeometry();
    bind(c, background, L"CornerRadius",
        L"Vector2(Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius), Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))",
        body_, shape_);
    bind(c, background, L"Offset", L"Vector2(0, -Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", body_, shape_);
    bind(c, background, L"Size",
        L"Vector2(pill.Size.X, pill.Size.Y + Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", body_, shape_);
    auto backgroundShape = c.CreateSpriteShape(background);
    fill_ = c.CreateColorBrush(fillColor(false));
    backgroundShape.FillBrush(fill_);
    body_.Shapes().Append(backgroundShape);

    // Content lives in a container clipped to the shape's live size, so new
    // content fading in during a grow morph never shows outside it.
    content_ = c.CreateContainerVisual();
    bind(c, content_, L"Size", L"pill.Size", body_, shape_);
    content_.Clip(c.CreateInsetClip());
    body_.Children().InsertAtTop(content_);
    elements_.init(c, device, content_);

    parent.Children().InsertAtTop(body_);

    // Attached last, so the initial Size/Offset above don't animate.
    refreshAnimations();
}

void Blob::refreshAnimations()
{
    implicit_ = morphAnimations(compositor_);
    body_.ImplicitAnimations(implicit_);
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
    if (fill_) fill_.Color(fillColor(highlighted_)); // colours are tokens too
    elements_.refresh();
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

void Blob::follow(std::optional<Vec2> centre)
{
    if (centre) {
        centre_ = *centre;
        drag_.InsertVector2(L"Centre", toFloat2(*centre));
        if (following_) return;
        following_ = true;
        auto anim = compositor_.CreateExpressionAnimation(L"Vector3(drag.Centre.X - pill.Size.X / 2, drag.Centre.Y - pill.Size.Y / 2, 0)");
        anim.SetReferenceParameter(L"pill", body_);
        anim.SetReferenceParameter(L"drag", drag_);
        body_.StartAnimation(L"Offset", anim);
        return;
    }
    if (!following_) return;
    // Let go: glide from where it was dropped to where it belongs. An explicit
    // animation with both ends, because stopping the cursor expression and
    // setting Offset left it stuck at the drop point on a real build.
    following_ = false;
    Vec2 from{ centre_.x - size_.x / 2, centre_.y - size_.y / 2 };
    body_.ImplicitAnimations(nullptr); // so setting the resting value below doesn't start a morph of its own
    body_.Offset(toFloat3(offset_));
    body_.ImplicitAnimations(implicit_);
    auto home = compositor_.CreateVector3KeyFrameAnimation();
    home.InsertKeyFrame(0.0f, toFloat3(from));
    home.InsertKeyFrame(1.0f, toFloat3(offset_), easingFor(compositor_, tokens().morphEase));
    home.Duration(tokens().morphDuration);
    body_.StartAnimation(L"Offset", home); // replaces the cursor expression
}

void Blob::setHighlighted(bool on)
{
    if (on == highlighted_) return;
    highlighted_ = on;
    auto anim = compositor_.CreateColorKeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, fillColor(on));
    anim.Duration(std::max(tokens().hoverFadeDuration, Ms{ 1 }));
    fill_.StartAnimation(L"Color", anim);
}

void Blob::setSelectedTab(std::optional<Rect> tab, float cornerRadius) { showMark(selectedTab_, tab, cornerRadius, tokens().tabSelectedColor, true); }

void Blob::setHovered(std::optional<Rect> area, float cornerRadius) { showMark(hovered_, area, cornerRadius, tokens().hoverColor, false); }

void Blob::setTiles(const std::vector<Rect>& tiles, float cornerRadius)
{
    if (tiles_.size() < tiles.size()) tiles_.resize(tiles.size());
    for (std::size_t i = 0; i < tiles_.size(); ++i)
        showMark(tiles_[i], i < tiles.size() ? std::optional<Rect>(tiles[i]) : std::nullopt, cornerRadius, tokens().tileColor, false);
}

void Blob::showMark(Mark& mark, std::optional<Rect> at, float cornerRadius, const Color& color, bool slide)
{
    if (at == mark.at) return;
    std::optional<Rect> from = std::exchange(mark.at, at);
    if (!mark.visual) {
        mark.shape = compositor_.CreateRoundedRectangleGeometry();
        auto fill = compositor_.CreateSpriteShape(mark.shape);
        mark.brush = compositor_.CreateColorBrush();
        fill.FillBrush(mark.brush);
        mark.visual = compositor_.CreateShapeVisual();
        mark.visual.Shapes().Append(fill);
        mark.visual.Opacity(0.0f);
        content_.Children().InsertAtBottom(mark.visual); // under the content it marks
    }
    mark.brush.Color(toUiColor(color)); // every time: tokens.json may have changed it
    if (!at) {
        fadeTo(compositor_, mark.visual, 0.0f, tokens().hoverFadeDuration);
        return;
    }
    mark.visual.Size({ at->width, at->height });
    mark.shape.Size({ at->width, at->height });
    mark.shape.CornerRadius({ cornerRadius, cornerRadius });
    mark.visual.Offset({ at->x, at->y, 0 });
    if (slide && from) {
        // Glide from the old tab to the new one: the one motion that says
        // "switched". Both ends explicit (decision 008).
        auto anim = compositor_.CreateVector3KeyFrameAnimation();
        anim.InsertKeyFrame(0.0f, { from->x, from->y, 0 });
        anim.InsertKeyFrame(1.0f, { at->x, at->y, 0 }, easingFor(compositor_, tokens().morphEase));
        anim.Duration(tokens().morphDuration);
        mark.visual.StartAnimation(L"Offset", anim);
    }
    fadeTo(compositor_, mark.visual, 1.0f, tokens().hoverFadeDuration);
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
