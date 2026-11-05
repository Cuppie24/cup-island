#include "platform/menu_layer.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"
#include "platform/element_parts.h"

#include <algorithm>

namespace cup::platform {

void MenuLayer::build(wuc::Compositor const& c, GraphicsDevice& device, wuc::ContainerVisual const& root)
{
    compositor_ = c;
    const Tokens& t = tokens();
    layer_ = c.CreateContainerVisual();
    layer_.Opacity(0.0f);
    root.Children().InsertAtTop(layer_); // over the island: not clipped by its shape (decision 086)
    shapes_ = c.CreateShapeVisual();
    layer_.Children().InsertAtBottom(shapes_);

    // The raised colour a toast is drawn in (decision 071), with a faint edge: over the card, it stands apart.
    background_ = c.CreateRoundedRectangleGeometry();
    fill_ = c.CreateColorBrush(toUiColor(t.islandHighlightColor));
    edge_ = c.CreateColorBrush(toUiColor(t.trackColor));
    auto panel = c.CreateSpriteShape(background_);
    panel.FillBrush(fill_);
    panel.StrokeBrush(edge_);
    panel.StrokeThickness(t.dividerThickness);
    shapes_.Shapes().Append(panel);

    lit_ = c.CreateRoundedRectangleGeometry();
    litFill_ = c.CreateColorBrush(toUiColor(t.buttonHoverColor));
    litShape_ = c.CreateSpriteShape(lit_);
    litShape_.FillBrush(litFill_);
    shapes_.Shapes().Append(litShape_);
    lineFill_ = c.CreateColorBrush(toUiColor(t.trackColor));
    lines_ = c.CreateShapeVisual();
    layer_.Children().InsertAbove(lines_, shapes_);
    elements_.init(c, device, layer_); // labels and shortcuts, above its shapes
}

void MenuLayer::show(const std::optional<MenuScene>& menu, Vec2 offset, Ms now)
{
    if (!menu) {
        elements_.apply({}, now);
        layer_.StopAnimation(L"Opacity");
        layer_.StopAnimation(L"Scale");
        layer_.Opacity(0.0f); // gone at once: what was chosen happens now
        shownRow_.reset();
        return;
    }
    const Tokens& t = tokens();
    Rect f = snapToPixels(menu->frame);
    // Its own coordinates: the frame's corner at 0, 0, so its content's frames move by it.
    std::vector<Placed> elements = menu->elements;
    for (Placed& p : elements) {
        p.frame.x -= f.x, p.frame.y -= f.y;
        p.hitFrame.x -= f.x, p.hitFrame.y -= f.y;
    }
    elements_.apply(elements, now);
    layer_.Offset({ offset.x + f.x, offset.y + f.y, 0 });
    layer_.Size({ f.width, f.height });
    shapes_.Size({ f.width, f.height });
    lines_.Size({ f.width, f.height });
    float half = t.dividerThickness / 2; // the edge's stroke inside the frame
    background_.Offset({ half, half });
    background_.Size({ std::max(0.0f, f.width - 2 * half), std::max(0.0f, f.height - 2 * half) });
    background_.CornerRadius({ menu->radius, menu->radius });

    if (menu->lit && *menu->lit < menu->items.size()) {
        Rect band = menu->items[*menu->lit];
        float r = std::max(0.0f, menu->radius - t.menuPadding); // nested in the menu's corners (decision 006)
        lit_.Offset({ band.x - f.x, band.y - f.y });
        lit_.Size({ band.width, band.height });
        lit_.CornerRadius({ r, r });
        litShape_.FillBrush(litFill_);
    } else {
        litShape_.FillBrush(nullptr);
    }
    lines_.Shapes().Clear(); // an IVector: no Remove by value
    for (const Rect& l : menu->lines) {
        auto geometry = compositor_.CreateRectangleGeometry();
        geometry.Offset({ l.x - f.x, l.y - f.y });
        geometry.Size({ l.width, l.height });
        auto line = compositor_.CreateSpriteShape(geometry);
        line.FillBrush(lineFill_);
        lines_.Shapes().Append(line);
    }

    if (shownRow_ == menu->activityId + "/" + menu->row) return; // already up: only what's lit changed
    shownRow_ = menu->activityId + "/" + menu->row;
    // Grows from where it opened (the pointer, or the row's side) as it fades in.
    layer_.CenterPoint({ menu->from.x - f.x, menu->from.y - f.y, 0 });
    Ms duration = std::max(t.menuOpenDuration, Ms{ 1 });
    auto ease = easingFor(compositor_, t.morphEase);
    auto opacity = compositor_.CreateScalarKeyFrameAnimation();
    opacity.InsertKeyFrame(0.0f, 0.0f);
    opacity.InsertKeyFrame(1.0f, 1.0f, ease);
    opacity.Duration(duration);
    auto scale = compositor_.CreateVector3KeyFrameAnimation();
    scale.InsertKeyFrame(0.0f, { t.contentArriveScale, t.contentArriveScale, 1.0f });
    scale.InsertKeyFrame(1.0f, { 1.0f, 1.0f, 1.0f }, ease);
    scale.Duration(duration);
    layer_.StartAnimation(L"Opacity", opacity);
    layer_.StartAnimation(L"Scale", scale);
}

void MenuLayer::refresh(bool coloursOnly)
{
    const Tokens& t = tokens();
    fadeColor(fill_, t.islandHighlightColor);
    fadeColor(edge_, t.trackColor);
    fadeColor(litFill_, t.buttonHoverColor);
    fadeColor(lineFill_, t.trackColor);
    elements_.refresh(coloursOnly);
}

} // namespace cup::platform
