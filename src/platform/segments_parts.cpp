#include "platform/segments_parts.h"

#include "core/motion.h"
#include "core/segments.h"
#include "core/tokens.h"
#include "platform/element_parts.h"

#include <algorithm>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

Color pillColor(const Segments& s) { return s.enabled ? tokens().buttonColor : tokens().buttonDisabledColor; }

// The picked option's pill, in the control's own coordinates (core's geometry, so clicks agree).
Rect pillFrame(Vec2 size, const Segments& segments) { return segmentFrame(segments, { 0, 0, size.x, size.y }, segments.picked); }

void shape(CompositionRoundedRectangleGeometry const& g, Rect f)
{
    g.Offset({ f.x, f.y });
    g.Size({ f.width, f.height });
    g.CornerRadius({ f.height / 2, f.height / 2 });
}

SpriteVisual spriteOf(Compositor const& c, CompositionDrawingSurface const& surface, Vec2 size)
{
    auto sprite = c.CreateSpriteVisual();
    sprite.Brush(c.CreateSurfaceBrush(surface));
    sprite.Size(toFloat2(size));
    return sprite;
}

} // namespace

SegmentsParts createSegments(Compositor const& c, CompositionDrawingSurface const& labels, CompositionDrawingSurface const& picked,
                             Vec2 size, const Segments& segments)
{
    SegmentsParts parts;
    parts.labels = labels;
    parts.picked = picked;
    parts.track = c.CreateColorBrush(toUiColor(tokens().islandColor));
    parts.pillBrush = c.CreateColorBrush(toUiColor(pillColor(segments)));

    auto track = c.CreateRoundedRectangleGeometry();
    track.Size(toFloat2(size));
    track.CornerRadius({ size.y / 2, size.y / 2 });
    auto trackShape = c.CreateSpriteShape(track);
    trackShape.FillBrush(parts.track);
    Rect at = pillFrame(size, segments);
    parts.pill = c.CreateRoundedRectangleGeometry();
    shape(parts.pill, at);
    auto pillShape = c.CreateSpriteShape(parts.pill);
    pillShape.FillBrush(parts.pillBrush);
    auto shapes = c.CreateShapeVisual();
    shapes.Size(toFloat2(size));
    shapes.Shapes().Append(trackShape);
    shapes.Shapes().Append(pillShape);

    // The dark labels show only inside the pill: a clip of the same shape, moved with it.
    auto inPill = spriteOf(c, picked, size);
    parts.pillClip = c.CreateRoundedRectangleGeometry();
    shape(parts.pillClip, at);
    inPill.Clip(c.CreateGeometricClip(parts.pillClip));

    parts.visual = c.CreateContainerVisual();
    parts.visual.Size(toFloat2(size));
    parts.visual.Children().InsertAtTop(shapes);
    parts.visual.Children().InsertAtTop(spriteOf(c, labels, size));
    parts.visual.Children().InsertAtTop(inPill);
    return parts;
}

void slideSegments(Compositor const& c, const SegmentsParts& parts, Vec2 size, const Segments& segments)
{
    const Tokens& t = tokens();
    Rect to = pillFrame(size, segments);
    Bezier settle = settleCurve(t); // the switch knob's (decision 080): lands just past and back
    for (const auto& g : { parts.pill, parts.pillClip }) {
        auto slide = c.CreateVector2KeyFrameAnimation();
        slide.InsertKeyFrame(1.0f, { to.x, to.y }, c.CreateCubicBezierEasingFunction(toFloat2(settle.c1), toFloat2(settle.c2)));
        slide.Duration(std::max(t.morphDuration, Ms{ 1 }));
        g.StartAnimation(L"Offset", slide); // from wherever it is: a quick second pick turns back smoothly
    }
    fadeColor(parts.pillBrush, pillColor(segments), t.morphDuration);
}

void recolorSegments(const SegmentsParts& parts, const Segments& segments)
{
    fadeColor(parts.track, tokens().islandColor);
    fadeColor(parts.pillBrush, pillColor(segments));
}

} // namespace cup::platform
