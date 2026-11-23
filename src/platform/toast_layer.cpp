#include "platform/toast_layer.h"

#include "core/motion.h"
#include "core/tokens.h"
#include "platform/blob_shape.h"
#include "platform/element_parts.h"
#include "platform/text_format.h"

#include <algorithm>

namespace cup::platform {

void ToastLayer::build(wuc::Compositor const& c, GraphicsDevice& device, wuc::ContainerVisual const& parent, wuc::Visual const& below)
{
    compositor_ = c;
    frame_ = c.CreateContainerVisual();
    frame_.Clip(c.CreateInsetClip()); // the card's edges: below the bottom one, the toast is out of sight
    parent.Children().InsertAbove(frame_, below);
    layer_ = c.CreateContainerVisual();
    layer_.Opacity(0.0f);
    frame_.Children().InsertAtTop(layer_);

    // The pill: the island's raised colour (the theme's), under the toast's text and button,
    // with the menu's faint edge and a shadow (decision 151): on a page of the same dark it stands apart.
    const Tokens& t = tokens();
    shape_ = c.CreateRoundedRectangleGeometry();
    fill_ = c.CreateColorBrush(toUiColor(t.islandHighlightColor));
    edge_ = c.CreateColorBrush(toUiColor(t.trackColor));
    auto pill = c.CreateSpriteShape(shape_);
    pill.FillBrush(fill_);
    pill.StrokeBrush(edge_);
    pill.StrokeThickness(t.dividerThickness);
    // Drawn straight into the tree, as the menu's panel is, so its edge is the shape's own.
    // (In a LayerVisual casting the shadow it came out fuzzy and stepped: decision 165.)
    // Soft: a window's tree is Hard, which leaves rounded corners aliased (decision 161).
    background_ = c.CreateShapeVisual();
    background_.BorderMode(wuc::CompositionBorderMode::Soft);
    background_.Shapes().Append(pill);
    layer_.Children().InsertAtBottom(background_);
    // Its shadow, under it. DropShadow.Mask is Windows 10 1607 (decision 015).
    device_ = &device;
    shadow_ = c.CreateDropShadow();
    applyShadow();
    caster_ = c.CreateSpriteVisual(); // no brush: only its shadow shows
    caster_.Shadow(shadow_);
    layer_.Children().InsertAtBottom(caster_);
    elements_.init(c, device, layer_); // its text and button, above the pill
}

void ToastLayer::show(const std::optional<ToastScene>& toast, Ms now)
{
    elements_.apply(toast ? toast->elements : std::vector<Placed>{}, now);
    if (toast) {
        Rect f = snapToPixels(toast->frame); // on the pixels its button and text are on
        background_.Offset({ f.x, f.y, 0 });
        background_.Size({ f.width, f.height });
        float half = tokens().dividerThickness / 2; // the edge's stroke inside the frame, not cut by it
        shape_.Offset({ half, half });
        shape_.Size({ std::max(0.0f, f.width - 2 * half), std::max(0.0f, f.height - 2 * half) });
        shape_.CornerRadius({ std::max(0.0f, toast->radius - half), std::max(0.0f, toast->radius - half) });
        caster_.Offset({ f.x, f.y, 0 });
        caster_.Size({ f.width, f.height });
        paintShadowMask({ f.width, f.height }, toast->radius);
    }
    if (toast) slide_ = toast->slide;
    if (toast.has_value() != shown_) animate(toast.has_value(), slide_); // a new one over a shown one just takes its place
    shown_ = toast.has_value();
}

// In: up from below the card's bottom edge into place, fading in, settling
// just past it (decision 080). Out: back down past it, fading out.
void ToastLayer::animate(bool in, float slide)
{
    const Tokens& t = tokens();
    Ms duration = std::max(t.toastSlideDuration, Ms{ 1 });
    auto ease = easingFor(compositor_, t.morphEase);
    auto settle = in ? easingFor(compositor_, settleCurve(t)) : ease;
    auto opacity = compositor_.CreateScalarKeyFrameAnimation();
    if (in) opacity.InsertKeyFrame(0.0f, 0.0f);
    opacity.InsertKeyFrame(1.0f, in ? 1.0f : 0.0f, ease);
    opacity.Duration(duration);
    auto offset = compositor_.CreateVector3KeyFrameAnimation();
    if (in) offset.InsertKeyFrame(0.0f, { 0.0f, slide, 0.0f });
    offset.InsertKeyFrame(1.0f, { 0.0f, in ? 0.0f : slide, 0.0f }, settle);
    offset.Duration(duration);
    layer_.StartAnimation(L"Opacity", opacity);
    layer_.StartAnimation(L"Offset", offset);
}

// The shadow's mask: opaque where the pill is, its round ends too. Whole pixels (the
// frame is snapped), so the surface maps onto the caster one to one.
void ToastLayer::paintShadowMask(Vec2 size, float radius)
{
    bool same = shadowMask_ && size.x == maskSize_.x && size.y == maskSize_.y && radius == maskRadius_;
    if (same && maskGeneration_ == device_->generation()) return;
    if (!same) {
        shadowMask_ = device_->createSurface(size);
        shadow_.Mask(compositor_.CreateSurfaceBrush(shadowMask_));
    }
    auto paint = [&](ID2D1DeviceContext* dc) {
        dc->FillRoundedRectangle(D2D1_ROUNDED_RECT{ D2D1::RectF(0, 0, size.x, size.y), radius, radius }, brushOf(dc, Color{ 0, 0, 0, 255 }).get());
    };
    // False: the GPU was lost and the device rebuilt (decision 164); once more on the new one.
    if (!device_->draw(shadowMask_, paint)) device_->draw(shadowMask_, paint);
    maskSize_ = size;
    maskRadius_ = radius;
    maskGeneration_ = device_->generation();
}

void ToastLayer::applyShadow()
{
    const Tokens& t = tokens();
    shadow_.BlurRadius(t.toastShadowBlur);
    shadow_.Offset(toFloat3(t.toastShadowOffset));
    shadow_.Color(toUiColor(t.toastShadowColor));
}

void ToastLayer::refresh(bool coloursOnly)
{
    fadeColor(fill_, tokens().islandHighlightColor);
    fadeColor(edge_, tokens().trackColor);
    applyShadow(); // a light theme's is fainter; tokens.json may have changed its size
    elements_.refresh(coloursOnly);
}

} // namespace cup::platform
