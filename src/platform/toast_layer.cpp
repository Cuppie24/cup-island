#include "platform/toast_layer.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"
#include "platform/element_parts.h"

#include <algorithm>

namespace cup::platform {

void ToastLayer::build(wuc::Compositor const& c, GraphicsDevice& device, wuc::ContainerVisual const& parent, wuc::Visual const& below)
{
    compositor_ = c;
    layer_ = c.CreateContainerVisual();
    layer_.Opacity(0.0f);
    parent.Children().InsertAbove(layer_, below);

    // The pill: the island's raised colour (the theme's), under the toast's text and button.
    shape_ = c.CreateRoundedRectangleGeometry();
    fill_ = c.CreateColorBrush(toUiColor(tokens().islandHighlightColor));
    auto pill = c.CreateSpriteShape(shape_);
    pill.FillBrush(fill_);
    background_ = c.CreateShapeVisual();
    background_.Shapes().Append(pill);
    layer_.Children().InsertAtBottom(background_);
    elements_.init(c, device, layer_); // its text and button, above the pill
}

void ToastLayer::show(const std::optional<ToastScene>& toast, Ms now)
{
    elements_.apply(toast ? toast->elements : std::vector<Placed>{}, now);
    if (toast) {
        const Rect& f = toast->frame;
        background_.Offset({ f.x, f.y, 0 });
        background_.Size({ f.width, f.height });
        shape_.Size({ f.width, f.height });
        shape_.CornerRadius({ toast->radius, toast->radius });
    }
    if (toast.has_value() != shown_) animate(toast.has_value()); // a new one over a shown one just takes its place
    shown_ = toast.has_value();
}

// In: it rises toastRise into place as it fades in. Out: it fades, sinking back a little.
void ToastLayer::animate(bool in)
{
    const Tokens& t = tokens();
    Ms duration = std::max(t.toastFadeDuration, Ms{ 1 });
    auto ease = easingFor(compositor_, t.morphEase);
    auto opacity = compositor_.CreateScalarKeyFrameAnimation();
    if (in) opacity.InsertKeyFrame(0.0f, 0.0f);
    opacity.InsertKeyFrame(1.0f, in ? 1.0f : 0.0f, ease);
    opacity.Duration(duration);
    auto offset = compositor_.CreateVector3KeyFrameAnimation();
    if (in) offset.InsertKeyFrame(0.0f, { 0.0f, t.toastRise, 0.0f });
    offset.InsertKeyFrame(1.0f, { 0.0f, in ? 0.0f : t.toastRise / 2, 0.0f }, ease);
    offset.Duration(duration);
    layer_.StartAnimation(L"Opacity", opacity);
    layer_.StartAnimation(L"Offset", offset);
}

void ToastLayer::refresh(bool coloursOnly)
{
    fadeColor(fill_, tokens().islandHighlightColor);
    elements_.refresh(coloursOnly);
}

} // namespace cup::platform
