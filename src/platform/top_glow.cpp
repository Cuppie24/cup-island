#include "platform/top_glow.h"

#include "core/tokens.h"
#include "platform/element_parts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cup::platform {
namespace {

// The glow's colour at the top edge: the theme's text colour, faint.
Color edgeColor()
{
    const Tokens& t = tokens();
    Color c = t.titleTextColor;
    c.a = static_cast<std::uint8_t>(std::lround(255 * std::clamp(t.collapseGlowOpacity, 0.0f, 1.0f)));
    return c;
}

Color clearOf(Color c)
{
    c.a = 0; // the same colour, clear: no grey band where it fades out
    return c;
}

} // namespace

void TopGlow::show(wuc::Compositor const& compositor, wuc::ContainerVisual const& content, std::optional<Rect> band)
{
    const Tokens& t = tokens();
    if (!visual_) {
        if (!band) return; // nothing to show yet
        auto brush = compositor.CreateLinearGradientBrush();
        brush.StartPoint({ 0.0f, 0.0f }); // top to bottom
        brush.EndPoint({ 0.0f, 1.0f });
        edge_ = compositor.CreateColorGradientStop(0.0f, toUiColor(edgeColor()));
        clear_ = compositor.CreateColorGradientStop(1.0f, toUiColor(clearOf(edgeColor())));
        brush.ColorStops().Append(edge_);
        brush.ColorStops().Append(clear_);
        visual_ = compositor.CreateSpriteVisual();
        visual_.Brush(brush);
        visual_.Opacity(0.0f);
        content.Children().InsertAtBottom(visual_); // under the content, like the tab and tile marks
    }
    if (band == at_) return;
    at_ = band;
    if (band) { // from the band's top, collapseGlowHeight down: past the band, so it fades softly
        visual_.Offset({ band->x, band->y, 0.0f });
        visual_.Size({ band->width, std::max(band->height, t.collapseGlowHeight) });
    }
    fadeTo(compositor, visual_, band ? 1.0f : 0.0f, t.hoverFadeDuration);
}

void TopGlow::recolor()
{
    if (!visual_) return;
    fadeColor(edge_, edgeColor());
    fadeColor(clear_, clearOf(edgeColor()));
}

} // namespace cup::platform
