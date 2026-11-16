#include "platform/top_glow.h"

#include "core/tokens.h"
#include "platform/blob_shape.h"
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
    // (Not named `small`: the Windows SDK's rpcndr.h has `#define small char`.)
    wfn::float3 sliver{ spread, spread, 1.0f }, whole{ 1.0f, 1.0f, 1.0f };
    if (!t.reducedMotion) {
        auto scale = c.CreateVector3KeyFrameAnimation();
        scale.InsertKeyFrame(1.0f, shown ? whole : sliver, ease); // from wherever it is: a quick in-and-out carries on smoothly
        scale.Duration(duration);
        visual.StartAnimation(L"Scale", scale);
    }
    auto opacity = c.CreateScalarKeyFrameAnimation();
    opacity.InsertKeyFrame(1.0f, shown ? 1.0f : 0.0f, ease);
    opacity.Duration(duration);
    visual.StartAnimation(L"Opacity", opacity);
}

} // namespace

void TopGlow::build(wuc::Compositor const& c, wuc::ShapeVisual const& body, wuc::CompositionPropertySet const& shape,
                    wuc::Visual const& content, wuc::CompositionVisualSurface const& outline)
{
    const Tokens& t = tokens();
    compositor_ = c;
    // From the top edge down collapseGlowHeight px, whatever the island's height: absolute, not a share of it.
    auto gradient = c.CreateLinearGradientBrush();
    gradient.MappingMode(wuc::CompositionMappingMode::Absolute);
    gradient.StartPoint({ 0.0f, 0.0f });
    gradient.EndPoint({ 0.0f, std::max(t.collapseGlowHeight, 1.0f) });
    edge_ = c.CreateColorGradientStop(0.0f, toUiColor(edgeColor()));
    clear_ = c.CreateColorGradientStop(1.0f, toUiColor(clearOf(edgeColor())));
    gradient.ColorStops().Append(edge_);
    gradient.ColorStops().Append(clear_);
    visual_ = c.CreateSpriteVisual();
    if (outline) {
        // Over the body and its ears, where the snapshot is (blob_backdrop), cut to its shape.
        auto cut = c.CreateMaskBrush();
        cut.Source(gradient);
        auto mask = c.CreateSurfaceBrush(outline);
        mask.Stretch(wuc::CompositionStretch::Fill);
        cut.Mask(mask);
        visual_.Brush(cut);
        bindToBlob(c, visual_, L"Size", L"Vector2(pill.Size.X + 2 * " + kEarSize + L", pill.Size.Y)", body, shape);
        bindToBlob(c, visual_, L"Offset", L"Vector3(-" + kEarSize + L", 0, 0)", body, shape);
        bindToBlob(c, visual_, L"CenterPoint", L"Vector3(pill.Size.X / 2 + " + kEarSize + L", 0, 0)", body, shape); // the top edge's middle
    } else {
        visual_.Brush(gradient); // no snapshot (before 1903): the body's rectangle
        bindToBlob(c, visual_, L"Size", L"pill.Size", body, shape);
        bindToBlob(c, visual_, L"CenterPoint", L"Vector3(pill.Size.X / 2, 0, 0)", body, shape);
    }
    visual_.Opacity(0.0f);
    float spread = std::clamp(t.collapseGlowSpread, 0.0f, 1.0f);
    visual_.Scale({ spread, spread, 1.0f }); // where it spreads from the first time
    body.Children().InsertBelow(visual_, content); // over the fill and the ears, under the content
}

void TopGlow::show(bool on)
{
    if (!visual_ || on == on_) return;
    on_ = on;
    animate(compositor_, visual_, on);
}

void TopGlow::recolor()
{
    if (!visual_) return;
    fadeColor(edge_, edgeColor());
    fadeColor(clear_, clearOf(edgeColor()));
}

} // namespace cup::platform
