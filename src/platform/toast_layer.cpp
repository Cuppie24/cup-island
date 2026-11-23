#include "platform/toast_layer.h"

#include "core/motion.h"
#include "core/tokens.h"
#include "platform/blob_shape.h"
#include "platform/element_parts.h"

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
    background_ = c.CreateShapeVisual();
    background_.Shapes().Append(pill);
    // In a layer, whose shadow takes the shape of what's drawn in it: the pill's rounded
    // corners, not its rectangle. LayerVisual.Shadow is Windows 10 1607 (decision 015).
    pillLayer_ = c.CreateLayerVisual();
    pillLayer_.Children().InsertAtTop(background_);
    shadow_ = c.CreateDropShadow();
    applyShadow();
    pillLayer_.Shadow(shadow_);
    layer_.Children().InsertAtBottom(pillLayer_);
    elements_.init(c, device, layer_); // its text and button, above the pill
}

void ToastLayer::setShiny()
{
    if (shine_.shape) return;
    const Tokens& t = tokens();
    shine_ = makeShine(compositor_, std::max(t.glassRimWidth, 1.0f) * 1.5f);
    background_.Shapes().Append(shine_.shape); // over the pill's own edge; placed with the next toast
    fadeShine(shine_, liquidShine(t), Ms{ 1 });
    elements_.setShiny(true); // its button too
}

void ToastLayer::show(const std::optional<ToastScene>& toast, Ms now)
{
    elements_.apply(toast ? toast->elements : std::vector<Placed>{}, now);
    if (toast) {
        Rect f = snapToPixels(toast->frame); // on the pixels its button and text are on
        pillLayer_.Offset({ f.x, f.y, 0 });
        pillLayer_.Size({ f.width, f.height });
        background_.Size({ f.width, f.height });
        shape_.Size({ f.width, f.height });
        shape_.CornerRadius({ toast->radius, toast->radius });
        placeShine(shine_, { 0, 0, f.width, f.height }, toast->radius);
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
    fadeShine(shine_, liquidShine(tokens()), recolorDuration(tokens()));
    applyShadow(); // a light theme's is fainter; tokens.json may have changed its size
    elements_.refresh(coloursOnly);
}

} // namespace cup::platform
