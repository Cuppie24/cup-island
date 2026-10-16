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

// Background: only the bottom corners are rounded (a notch hanging from the
// screen edge). The geometry has one radius for all corners, so it's made a
// radius taller and shifted up; ShapeVisual clips the top corners away.
// Radius = half the height, capped by maxCornerRadius: the same curve as core
// pillCornerRadius(), which content radii are nested in. Dragged off the edge
// (Attached 0) the top corners round too: the square bubble becomes a circle
// (decision 025). Built on `target`, whose Size is the body's.
void addBackground(Compositor const& c, ShapeVisual const& target, CompositionPropertySet const& shape, CompositionBrush const& fill)
{
    auto background = c.CreateRoundedRectangleGeometry();
    bind(c, background, L"CornerRadius",
        L"Vector2(Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius), Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))",
        target, shape);
    bind(c, background, L"Offset", L"Vector2(0, -shape.Attached * Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", target, shape);
    bind(c, background, L"Size",
        L"Vector2(pill.Size.X, pill.Size.Y + shape.Attached * Min(Min(pill.Size.X, pill.Size.Y) / 2, shape.MaxRadius))", target, shape);
    auto backgroundShape = c.CreateSpriteShape(background);
    backgroundShape.FillBrush(fill);
    target.Shapes().Append(backgroundShape);
}

// The fill's two ends: the highlight's colour (a drop target, decision 007) on both, or the island's.
Color leftColor(bool highlighted) { return highlighted ? tokens().islandHighlightColor : tokens().islandColor; }
Color rightColor(bool highlighted) { return highlighted ? tokens().islandHighlightColor : tokens().islandSecondColor; }

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

    leftFill_ = c.CreateColorBrush(toUiColor(leftColor(false)));
    rightFill_ = c.CreateColorBrush(toUiColor(rightColor(false)));
    leftStop_ = c.CreateColorGradientStop(0.0f, toUiColor(leftColor(false)));
    rightStop_ = c.CreateColorGradientStop(1.0f, toUiColor(rightColor(false)));
    gradient_ = c.CreateLinearGradientBrush(); // relative to the background's bounds: its left edge to its right
    gradient_.StartPoint({ 0.0f, 0.5f });
    gradient_.EndPoint({ 1.0f, 0.5f });
    gradient_.ColorStops().Append(leftStop_);
    gradient_.ColorStops().Append(rightStop_);
    addBackground(c, body_, shape_, gradient_);
    addEars(c, body_, shape_, leftFill_, rightFill_); // the flared top corners (decision 025)

    // Content lives in a container clipped to the shape's live size, so new
    // content fading in during a grow morph never shows outside it.
    content_ = c.CreateContainerVisual();
    bind(c, content_, L"Size", L"pill.Size", body_, shape_);
    content_.Clip(c.CreateInsetClip());
    body_.Children().InsertAtTop(content_);
    badges_.build(c, body_, content_); // dots over the content (decision 038)
    elements_.init(c, device, content_);

    buildShadow(c, parent); // under the body
    parent.Children().InsertAtTop(body_);

    // Attached last, so the initial Size/Offset above don't animate.
    refreshAnimations(false);
}

void Blob::buildShadow(Compositor const& c, ContainerVisual const& parent)
{
    // CreateVisualSurface is Windows 10 1903+; on 1809 the island just has no
    // shadow (decision 015: newer APIs only behind runtime checks).
    using winrt::Windows::Foundation::Metadata::ApiInformation;
    if (!ApiInformation::IsMethodPresent(L"Windows.UI.Composition.Compositor", L"CreateVisualSurface")) return;

    // The snapshot is of an outline-only twin of the body: the same background
    // and ears, no content. Snapshotting the body itself re-rendered and
    // re-blurred the shadow on every frame of anything animating inside it
    // (the equalizer, a swimming title, the progress bar): all the time music
    // played, which was enough GPU load to make audio stutter. The twin only
    // changes when the shape does (morph, press, drag). It sits right under
    // the body with the same fill and transform, so it's never seen.
    outline_ = c.CreateShapeVisual();
    bind(c, outline_, L"Size", L"pill.Size", body_, shape_);
    bind(c, outline_, L"Offset", L"pill.Offset", body_, shape_);
    bind(c, outline_, L"CenterPoint", L"pill.CenterPoint", body_, shape_);
    bind(c, outline_, L"Scale", L"pill.Scale", body_, shape_);
    addBackground(c, outline_, shape_, gradient_);
    addEars(c, outline_, shape_, leftFill_, rightFill_);

    auto snapshot = c.CreateVisualSurface(); // live: re-rendered as the outline changes, on the compositor
    snapshot.SourceVisual(outline_);
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
    parent.Children().InsertAtTop(outline_); // above the shadow, below the body (inserted next)
}

void Blob::applyShadow()
{
    if (!shadow_) return;
    const Tokens& t = tokens();
    shadow_.Color(toUiColor(t.shadowColor));
    shadow_.BlurRadius(t.shadowBlur);
    shadow_.Offset(toFloat3(t.shadowOffset));
}

void Blob::refreshAnimations(bool coloursOnly)
{
    const Tokens& t = tokens();
    implicit_ = morphAnimations(compositor_);
    body_.ImplicitAnimations(implicit_);
    if (shadow_) {
        shadow_.BlurRadius(t.shadowBlur);
        shadow_.Offset(toFloat3(t.shadowOffset));
    }
    shape_.InsertScalar(L"MaxRadius", t.maxCornerRadius);
    shape_.InsertScalar(L"FlareRadius", t.flareRadius);

    // Colours are tokens too, and every one fades, so a cover's colours or a
    // new theme ease in (decision 051): the fill, the shadow, the marks.
    if (gradient_) fadeFill(recolorDuration(t));
    if (shadow_) fadeColor(shadow_, t.shadowColor);
    for (Mark* mark : { &selectedTab_, &pinnedTab_ })
        if (mark->brush) fadeColor(mark->brush, t.tabSelectedColor);
    if (hovered_.brush) fadeColor(hovered_.brush, t.hoverColor);
    for (std::vector<Mark>* marks : { &tiles_, &tabs_ })
        for (Mark& mark : *marks)
            if (mark.brush) fadeColor(mark.brush, t.tileColor);
    for (Mark& mark : selected_)
        if (mark.brush) fadeColor(mark.brush, t.tabSelectedColor);
    if (box_.brush) fadeColor(box_.brush, t.selectionBoxColor);
    if (focusRing_.brush) fadeColor(focusRing_.brush, t.focusRingColor);
    badges_.recolor();
    elements_.refresh(coloursOnly);
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
    fadeFill(tokens().hoverFadeDuration);
}

void Blob::fadeFill(Ms duration)
{
    Color left = leftColor(highlighted_), right = rightColor(highlighted_);
    fadeColor(leftStop_, left, duration);
    fadeColor(leftFill_, left, duration);
    fadeColor(rightStop_, right, duration);
    fadeColor(rightFill_, right, duration);
}

void Blob::setSelectedTab(std::optional<Rect> tab, float cornerRadius) { showMark(selectedTab_, tab, cornerRadius, tokens().tabSelectedColor, true); }

void Blob::setPinnedTab(std::optional<Rect> pin, float cornerRadius) { showMark(pinnedTab_, pin, cornerRadius, tokens().tabSelectedColor, false); }

void Blob::setHovered(std::optional<Rect> area, float cornerRadius) { showMark(hovered_, area, cornerRadius, tokens().hoverColor, false); }

void Blob::setFocusRing(std::optional<Rect> area, float cornerRadius)
{
    Ring& ring = focusRing_;
    if (!ring.visual) {
        ring.shape = compositor_.CreateRoundedRectangleGeometry();
        ring.stroke = compositor_.CreateSpriteShape(ring.shape);
        ring.brush = compositor_.CreateColorBrush(toUiColor(tokens().focusRingColor)); // refreshAnimations() fades it
        ring.stroke.StrokeBrush(ring.brush);
        ring.visual = compositor_.CreateShapeVisual();
        ring.visual.Shapes().Append(ring.stroke);
        ring.visual.Opacity(0.0f);
        // Above the content, which adds its visuals at the top of content_ as
        // they come: a sibling over it stays over them all.
        body_.Children().InsertAbove(ring.visual, content_);
    }
    float width = tokens().focusRingWidth;
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

void Blob::showMarks(std::vector<Mark>& marks, const std::vector<Rect>& at, float cornerRadius, const Color& color, bool instant)
{
    if (marks.size() < at.size()) marks.resize(at.size());
    for (std::size_t i = 0; i < marks.size(); ++i)
        showMark(marks[i], i < at.size() ? std::optional<Rect>(at[i]) : std::nullopt, cornerRadius, color, false, instant);
}

void Blob::showMark(Mark& mark, std::optional<Rect> at, float cornerRadius, const Color& color, bool slide, bool instant)
{
    if (at == mark.at) return;
    std::optional<Rect> from = std::exchange(mark.at, at);
    if (!mark.visual) {
        mark.shape = compositor_.CreateRoundedRectangleGeometry();
        auto fill = compositor_.CreateSpriteShape(mark.shape);
        mark.brush = compositor_.CreateColorBrush(toUiColor(color)); // refreshAnimations() fades it
        fill.FillBrush(mark.brush);
        mark.visual = compositor_.CreateShapeVisual();
        mark.visual.Shapes().Append(fill);
        mark.visual.Opacity(0.0f);
        content_.Children().InsertAtBottom(mark.visual); // under the content it marks
    }
    if (!at) {
        if (instant) {
            mark.visual.StopAnimation(L"Opacity");
            mark.visual.Opacity(0.0f);
        } else {
            fadeTo(compositor_, mark.visual, 0.0f, tokens().hoverFadeDuration);
        }
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
    if (instant) {
        mark.visual.StopAnimation(L"Opacity");
        mark.visual.Opacity(1.0f);
    } else {
        fadeTo(compositor_, mark.visual, 1.0f, tokens().hoverFadeDuration);
    }
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
