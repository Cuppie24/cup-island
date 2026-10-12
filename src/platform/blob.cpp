#include "platform/blob.h"

#include "platform/blob_shape.h"
#include "platform/element_parts.h"

#include <winrt/Windows.Foundation.Metadata.h>

#include <cmath>
#include <algorithm>
#include <string>
#include <utility>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Properties bound to the body's live (animating) size follow every morph
// without app code running per frame (blob_shape.h).
constexpr auto bind = bindToBlob;

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
    drag_.InsertScalar(L"Drop", 0.0f); // lifted: how far below the screen edge (decision 026)

    // Press-scale pivots on the middle of the top edge, so the shape stays
    // glued to the screen edge while its sides and bottom squash in.
    shape_ = c.CreatePropertySet();
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
    shape_.InsertScalar(L"FlareRadius", tokens().flareRadius);
    shape_.InsertScalar(L"Attached", 1.0f); // hanging from the screen edge: a straight top
    shape_.InsertScalar(L"Flare", 1.0f);    // ...and flared into it (decision 025)
    shape_.InsertScalar(L"LeftEar", 1.0f);  // per side: 0 when flush with the screen's edge (decision 026)
    shape_.InsertScalar(L"RightEar", 1.0f);
    bind(c, body_, L"CenterPoint", L"Vector3(pill.Size.X / 2, 0, 0)", body_, shape_);

    // Background: only the bottom corners are rounded (a notch hanging from the
    // screen edge). The geometry has one radius for all corners, so it's made
    // a radius taller and shifted up; ShapeVisual clips the top corners away.
    // Radius = half the height, capped by maxCornerRadius: the same curve as
    // core pillCornerRadius(), which content radii are nested in. Dragged off
    // the edge (Attached 0) the top corners round too: the square bubble
    // becomes a circle (decision 025).
    auto background = c.CreateRoundedRectangleGeometry();
    bind(c, background, L"CornerRadius",
        L"Vector2(Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius), Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))",
        body_, shape_);
    bind(c, background, L"Offset", L"Vector2(0, -shape.Attached * Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", body_, shape_);
    bind(c, background, L"Size",
        L"Vector2(pill.Size.X, pill.Size.Y + shape.Attached * Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", body_, shape_);
    auto backgroundShape = c.CreateSpriteShape(background);
    fill_ = c.CreateColorBrush(fillColor(false));
    backgroundShape.FillBrush(fill_);
    body_.Shapes().Append(backgroundShape);
    addEars(c, body_, shape_, fill_); // the flared top corners (decision 025)

    // Content lives in a container clipped to the shape's live size, so new
    // content fading in during a grow morph never shows outside it.
    content_ = c.CreateContainerVisual();
    bind(c, content_, L"Size", L"pill.Size", body_, shape_);
    content_.Clip(c.CreateInsetClip());
    body_.Children().InsertAtTop(content_);
    elements_.init(c, device, content_);

    buildShadow(c, parent); // under the body
    parent.Children().InsertAtTop(body_);

    // Attached last, so the initial Size/Offset above don't animate.
    refreshAnimations();
}

void Blob::buildShadow(Compositor const& c, ContainerVisual const& parent)
{
    // CreateVisualSurface is Windows 10 1903+; on 1809 the island just has no
    // shadow (decision 015: newer APIs only behind runtime checks).
    using winrt::Windows::Foundation::Metadata::ApiInformation;
    if (!ApiInformation::IsMethodPresent(L"Windows.UI.Composition.Compositor", L"CreateVisualSurface")) return;

    auto snapshot = c.CreateVisualSurface(); // live: re-rendered as the body changes, on the compositor
    snapshot.SourceVisual(body_);
    // As wide as the body plus its ears (decision 025), so the shadow follows their curve.
    bind(c, snapshot, L"SourceOffset", L"Vector2(-" + kEarSize + L", 0)", body_, shape_);
    bind(c, snapshot, L"SourceSize", L"Vector2(pill.Size.X + 2 * " + kEarSize + L", pill.Size.Y)", body_, shape_);
    shadow_ = c.CreateDropShadow();
    shadow_.Mask(c.CreateSurfaceBrush(snapshot)); // the body's alpha: its rounded shape, not its rectangle
    applyShadow();

    caster_ = c.CreateSpriteVisual(); // no brush: only its shadow shows
    bind(c, caster_, L"Size", L"Vector2(pill.Size.X + 2 * " + kEarSize + L", pill.Size.Y)", body_, shape_);
    bind(c, caster_, L"Offset", L"pill.Offset - Vector3(" + kEarSize + L", 0, 0)", body_, shape_);
    bind(c, caster_, L"CenterPoint", L"pill.CenterPoint + Vector3(" + kEarSize + L", 0, 0)", body_, shape_);
    bind(c, caster_, L"Scale", L"pill.Scale", body_, shape_);
    caster_.Shadow(shadow_);
    parent.Children().InsertAtTop(caster_);
}

void Blob::applyShadow()
{
    if (!shadow_) return;
    const Tokens& t = tokens();
    shadow_.Color(toUiColor(t.shadowColor));
    shadow_.BlurRadius(t.shadowBlur);
    shadow_.Offset(toFloat3(t.shadowOffset));
}

void Blob::refreshAnimations()
{
    implicit_ = morphAnimations(compositor_);
    body_.ImplicitAnimations(implicit_);
    applyShadow();
    shape_.InsertScalar(L"MaxRadius", tokens().maxCornerRadius);
    shape_.InsertScalar(L"FlareRadius", tokens().flareRadius);
    if (fill_) fill_.Color(fillColor(highlighted_)); // colours are tokens too
    elements_.refresh();
}

void Blob::place(Vec2 size, Vec2 offset)
{
    // At rest on whole pixels, so content inside isn't drawn between pixels
    // (blurry text). Rounded before the comparison below, so an unchanged
    // position still isn't re-set (decision 008).
    offset = { std::round(offset.x), std::round(offset.y) };
    // Only touch geometry on a real change: re-setting a property restarts
    // its implicit animation, which would stall a morph in flight. While
    // following, Offset belongs to the cursor expression; it lands later.
    if (size != size_) body_.Size(toFloat2(size));
    if (offset != offset_ && !following_) body_.Offset(toFloat3(offset));
    size_ = size;
    offset_ = offset;
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

void Blob::setFocusRing(std::optional<Rect> area, float cornerRadius)
{
    Ring& ring = focusRing_;
    if (!ring.visual) {
        ring.shape = compositor_.CreateRoundedRectangleGeometry();
        ring.stroke = compositor_.CreateSpriteShape(ring.shape);
        ring.brush = compositor_.CreateColorBrush();
        ring.stroke.StrokeBrush(ring.brush);
        ring.visual = compositor_.CreateShapeVisual();
        ring.visual.Shapes().Append(ring.stroke);
        ring.visual.Opacity(0.0f);
        // Above the content, which adds its visuals at the top of content_ as
        // they come: a sibling over it stays over them all.
        body_.Children().InsertAbove(ring.visual, content_);
    }
    float width = tokens().focusRingWidth;
    ring.brush.Color(toUiColor(tokens().focusRingColor)); // every time: tokens.json may have changed them
    ring.stroke.StrokeThickness(width);
    if (area == ring.at && cornerRadius == ring.radius) return;
    ring.at = area;
    ring.radius = cornerRadius;
    if (!area) {
        fadeTo(compositor_, ring.visual, 0.0f, tokens().hoverFadeDuration);
        return;
    }
    // A stroke is centred on its outline: inset by half its width, it's drawn
    // inside the frame and never clipped by the card's edge or a neighbour.
    float half = width / 2;
    ring.visual.Size({ area->width, area->height });
    ring.visual.Offset({ area->x, area->y, 0 });
    ring.shape.Offset({ half, half });
    ring.shape.Size({ std::max(0.0f, area->width - width), std::max(0.0f, area->height - width) });
    ring.shape.CornerRadius({ std::max(0.0f, cornerRadius - half), std::max(0.0f, cornerRadius - half) });
    // It moves at once, like hover, and only fades: no glide to hand off (decision 008).
    fadeTo(compositor_, ring.visual, 1.0f, tokens().hoverFadeDuration);
}

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
