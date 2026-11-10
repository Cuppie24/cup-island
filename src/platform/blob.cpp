#include "platform/blob.h"

#include "platform/blob_shape.h"
#include "platform/element_parts.h"
#include "platform/spring_motion.h"

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

// The fill's two ends: the highlight's colour (a drop target, decision 007) on both, or the island's.
Color leftColor(bool highlighted) { return highlighted ? tokens().islandHighlightColor : tokens().islandColor; }
Color rightColor(bool highlighted) { return highlighted ? tokens().islandHighlightColor : tokens().islandSecondColor; }

ShapeFill makeFill(Compositor const& c, Color left, Color right)
{
    ShapeFill f;
    f.leftEar = c.CreateColorBrush(toUiColor(left));
    f.rightEar = c.CreateColorBrush(toUiColor(right));
    f.left = c.CreateColorGradientStop(0.0f, toUiColor(left));
    f.right = c.CreateColorGradientStop(1.0f, toUiColor(right));
    f.gradient = c.CreateLinearGradientBrush(); // relative to the background's bounds: its left edge to its right
    f.gradient.StartPoint({ 0.0f, 0.5f });
    f.gradient.EndPoint({ 1.0f, 0.5f });
    f.gradient.ColorStops().Append(f.left);
    f.gradient.ColorStops().Append(f.right);
    return f;
}

void fade(ShapeFill& f, Color left, Color right, Ms duration)
{
    fadeColor(f.left, left, duration);
    fadeColor(f.leftEar, left, duration);
    fadeColor(f.right, right, duration);
    fadeColor(f.rightEar, right, duration);
}

} // namespace

void Blob::build(wuc::Compositor const& c, GraphicsDevice& device, wuc::ContainerVisual const& parent, Vec2 size, Vec2 offset)
{
    compositor_ = c;
    device_ = &device;
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
    shape_.InsertScalar(L"Overlap", 1.0f);  // ears reach this far into the body: 0 as glass (blob_shape.cpp)
    shape_.InsertScalar(L"RimInset", 0.0f); // half the glass rim's width (decision 089)
    shape_.InsertScalar(L"GlowHeight", tokens().glassGlowHeight); // the glow's reach (decision 090)
    bind(c, body_, L"CenterPoint", L"Vector3(pill.Size.X / 2, 0, 0)", body_, shape_);

    const Tokens& t = tokens();
    fill_ = makeFill(c, islandFill(t, leftColor(false)), islandFill(t, rightColor(false)));
    solid_ = makeFill(c, leftColor(false), rightColor(false));
    auto background = addBackground(c, body_, shape_, fill_.gradient);
    buildGlow(c, background); // over the fill: the cover's colour from the bottom edge (decision 090)
    buildRim(c);              // over both; the ears and content over it
    addEars(c, body_, shape_, fill_.leftEar, fill_.rightEar); // the flared top corners (decision 025)

    // Content lives in a container clipped to the shape's live size, so new
    // content fading in during a grow morph never shows outside it.
    content_ = c.CreateContainerVisual();
    bind(c, content_, L"Size", L"pill.Size", body_, shape_);
    content_.Clip(c.CreateInsetClip());
    body_.Children().InsertAtTop(content_);
    badges_.build(c, body_, content_); // dots over the content (decision 038)
    toast_.build(c, device, body_, content_); // a toast floats over the content too (decision 071)
    bind(c, toast_.frame(), L"Size", L"pill.Size", body_, shape_); // clipped to the card, like the content
    elements_.init(c, device, content_);

    buildShadow(c, parent); // under the body
    parent.Children().InsertAtTop(body_);

    // Attached last, so the initial Size/Offset above don't animate.
    refreshAnimations(false);
}

void Blob::refreshAnimations(bool coloursOnly)
{
    const Tokens& t = tokens();
    implicit_ = springMorph(compositor_, motion_); // tokens.json may have changed the spring
    built_ = motion_;
    body_.ImplicitAnimations(implicit_);
    if (shadow_) {
        shadow_.BlurRadius(t.shadowBlur);
        shadow_.Offset(toFloat3(t.shadowOffset));
    }
    shape_.InsertScalar(L"MaxRadius", detachedRadius_.value_or(t.maxCornerRadius)); // a floating card's are a window's
    shape_.InsertScalar(L"FlareRadius", t.flareRadius);
    shape_.InsertScalar(L"GlowHeight", t.glassGlowHeight);

    // Colours are tokens too, and every one fades, so a cover's colours or a
    // new theme ease in (decision 051): the fill, the shadow, the marks. Glass
    // on or off fades the same way (decision 089).
    applyGlass(recolorDuration(t)); // first: whether the blur is evened out sets the fill's strength
    fadeFill(recolorDuration(t));
    fadeGlow(recolorDuration(t));
    if (shadow_) fadeColor(shadow_, t.glass ? t.glassShadowColor : t.shadowColor);
    for (Mark* mark : { &selectedTab_, &pinnedTab_ })
        if (mark->brush) fadeColor(mark->brush, t.tabSelectedColor);
    if (hovered_.brush) fadeColor(hovered_.brush, t.hoverColor);
    for (Mark& mark : tiles_)
        if (mark.brush) fadeColor(mark.brush, t.tileColor);
    if (focusRing_.brush) fadeColor(focusRing_.brush, t.focusRingColor);
    badges_.recolor();
    toast_.refresh(coloursOnly);
    elements_.refresh(coloursOnly);
}

void Blob::place(Vec2 size, Vec2 offset, const Motion& motion)
{
    // At rest on whole pixels, so content inside isn't drawn between pixels
    // (blurry text). Rounded before the comparison below, so an unchanged
    // position still isn't re-set (decision 008).
    offset = { std::round(offset.x), std::round(offset.y) };
    // Only touch geometry on a real change: re-setting a property restarts
    // its implicit animation, which would stall a morph in flight. While
    // following, Offset belongs to the cursor expression; it lands later.
    motion_ = towardSize(motion, size);
    bool resize = size != size_, move = offset != offset_ && !following_;
    if ((resize || move) && motion_ != built_) {
        // Each morph has its own timing (decision 080): the implicit
        // animations are swapped before the change starts one. One already
        // running carries on; the new one starts from where it got to.
        implicit_ = springMorph(compositor_, motion_);
        built_ = motion_;
        body_.ImplicitAnimations(implicit_);
    }
    if (resize) body_.Size(toFloat2(size));
    if (move) body_.Offset(toFloat3(offset));
    size_ = size;
    offset_ = offset;
}

void Blob::setHighlighted(bool on)
{
    if (on == highlighted_) return;
    highlighted_ = on;
    fadeFill(tokens().hoverFadeDuration);
}

void Blob::setCard(bool on)
{
    if (on == card_) return;
    card_ = on;
    fadeFill(tokens().morphDuration);
    fadeGlow(tokens().morphDuration);
}

void Blob::fadeFill(Ms duration)
{
    Color left = leftColor(highlighted_), right = rightColor(highlighted_);
    // An open card as glass with a cover's colours: its own, one colour (decision 090).
    if (card_ && !highlighted_ && tokens().islandCardColor.a != 0) left = right = tokens().islandCardColor;
    // See-through as glass; more strongly over the plain blur (decision 089).
    bool even = glassEven_ || !glass_;
    fade(fill_, islandFill(tokens(), left, even), islandFill(tokens(), right, even), duration);
    fade(solid_, left, right, duration);
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

void Blob::showMarks(std::vector<Mark>& marks, const std::vector<Rect>& at, float cornerRadius)
{
    if (marks.size() < at.size()) marks.resize(at.size());
    for (std::size_t i = 0; i < marks.size(); ++i)
        showMark(marks[i], i < at.size() ? std::optional<Rect>(at[i]) : std::nullopt, cornerRadius, tokens().tileColor, false);
}

void Blob::showMark(Mark& mark, std::optional<Rect> at, float cornerRadius, const Color& color, bool slide)
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
        fadeTo(compositor_, mark.visual, 0.0f, tokens().hoverFadeDuration);
        return;
    }
    at = snapToPixels(*at); // on the pixels the content it marks is on (decision 071)
    mark.visual.Size({ at->width, at->height });
    mark.shape.Size({ at->width, at->height });
    mark.shape.CornerRadius({ cornerRadius, cornerRadius });
    mark.visual.Offset({ at->x, at->y, 0 });
    if (slide && from) {
        // Glide from the old tab to the new one: the one motion that says
        // "switched". Both ends explicit (decision 008).
        auto anim = compositor_.CreateVector3KeyFrameAnimation();
        anim.InsertKeyFrame(0.0f, { from->x, from->y, 0 });
        anim.InsertKeyFrame(1.0f, { at->x, at->y, 0 }, easingFor(compositor_, settleCurve(tokens()))); // just past, and back (decision 080)
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

void Blob::pulse()
{
    const Tokens& t = tokens();
    body_.StartAnimation(L"Scale", pulseScale(compositor_, t.alertPulseScale, { t.alertPulseDuration, t.morphGrowBounce, t.morphSpringSettle }));
}

void Blob::playScale(float scale, Ms duration, Bezier curve)
{
    auto anim = compositor_.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { scale, scale, 1.0f }, easingFor(compositor_, curve));
    anim.Duration(duration);
    body_.StartAnimation(L"Scale", anim); // replaces whatever was animating Scale
}

} // namespace cup::platform
