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

// Composition rejects zero-length animations.
Ms atLeast1(Ms d) { return std::max(d, Ms{ 1 }); }

// Shown: from a sliver in the top edge's middle to its full size, fading in;
// hidden: back toward it, fading out. About its top middle (its CenterPoint),
// so it spreads sideways and reaches down from the edge, never off it.
void animate(wuc::Compositor const& c, wuc::SpriteVisual const& visual, bool shown)
{
    const Tokens& t = tokens();
    Ms duration = atLeast1(t.reducedMotion ? t.hoverFadeDuration : t.collapseGlowDuration);
    auto ease = c.CreateCubicBezierEasingFunction(toFloat2(t.morphEase.c1), toFloat2(t.morphEase.c2));
    float spread = std::clamp(t.collapseGlowSpread, 0.0f, 1.0f);
    wfn::float3 small{ spread, spread, 1.0f }, full{ 1.0f, 1.0f, 1.0f };
    if (!t.reducedMotion) {
        auto scale = c.CreateVector3KeyFrameAnimation();
        scale.InsertKeyFrame(1.0f, shown ? full : small, ease); // from wherever it is: a quick in-and-out carries on smoothly
        scale.Duration(duration);
        visual.StartAnimation(L"Scale", scale);
    }
    auto opacity = c.CreateScalarKeyFrameAnimation();
    opacity.InsertKeyFrame(1.0f, shown ? 1.0f : 0.0f, ease);
    opacity.Duration(duration);
    visual.StartAnimation(L"Opacity", opacity);
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
        float spread = std::clamp(tokens().collapseGlowSpread, 0.0f, 1.0f);
        visual_.Scale({ spread, spread, 1.0f }); // where it spreads from the first time
        content.Children().InsertAtBottom(visual_); // under the content, like the tab and tile marks
    }
    if (band == at_) return;
    bool wasShown = at_.has_value();
    at_ = band;
    if (band) { // from the band's top, collapseGlowHeight down: past the band, so it fades softly
        visual_.Offset({ band->x, band->y, 0.0f });
        visual_.Size({ band->width, std::max(band->height, t.collapseGlowHeight) });
        visual_.CenterPoint({ band->width / 2, 0.0f, 0.0f }); // the top edge's middle
    }
    if (band.has_value() != wasShown) animate(compositor, visual_, band.has_value()); // the island resized under it: it stays lit
}

void TopGlow::recolor()
{
    if (!visual_) return;
    fadeColor(edge_, edgeColor());
    fadeColor(clear_, clearOf(edgeColor()));
}

} // namespace cup::platform
