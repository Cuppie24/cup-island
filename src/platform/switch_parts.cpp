#include "platform/switch_parts.h"

#include "core/tokens.h"
#include "platform/element_parts.h"

#include <algorithm>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// The knob's centre at either end, in the track's own coordinates.
wfn::float2 knobCentre(Vec2 frame, bool on)
{
    float r = frame.y / 2;
    return { on ? frame.x - r : r, r };
}

} // namespace

Color switchTrackColor(bool on) { return on ? tokens().liveColor : tokens().trackColor; }
Color switchKnobColor(bool on) { return on ? tokens().islandColor : tokens().knobColor; }

SwitchParts createSwitch(Compositor const& c, Vec2 frame, bool on)
{
    SwitchParts parts;
    parts.track = c.CreateColorBrush(toUiColor(switchTrackColor(on)));
    parts.knobBrush = c.CreateColorBrush(toUiColor(switchKnobColor(on)));

    auto track = c.CreateRoundedRectangleGeometry();
    track.Size(toFloat2(frame));
    track.CornerRadius({ frame.y / 2, frame.y / 2 });
    auto trackShape = c.CreateSpriteShape(track);
    trackShape.FillBrush(parts.track);

    float r = std::max(0.0f, frame.y / 2 - tokens().switchKnobInset);
    parts.knob = c.CreateEllipseGeometry();
    parts.knob.Radius({ r, r });
    parts.knob.Center(knobCentre(frame, on));
    auto knobShape = c.CreateSpriteShape(parts.knob);
    knobShape.FillBrush(parts.knobBrush);

    parts.visual = c.CreateShapeVisual();
    parts.visual.Shapes().Append(trackShape);
    parts.visual.Shapes().Append(knobShape);
    return parts;
}

void slideSwitch(Compositor const& c, const SwitchParts& parts, Vec2 frame, bool on)
{
    const Tokens& t = tokens();
    auto slide = c.CreateVector2KeyFrameAnimation();
    slide.InsertKeyFrame(1.0f, knobCentre(frame, on), c.CreateCubicBezierEasingFunction(toFloat2(t.morphEase.c1), toFloat2(t.morphEase.c2)));
    slide.Duration(std::max(t.morphDuration, Ms{ 1 }));
    parts.knob.StartAnimation(L"Center", slide); // from wherever it is: a quick double click turns back smoothly
    fadeColor(parts.track, switchTrackColor(on), t.morphDuration);
    fadeColor(parts.knobBrush, switchKnobColor(on), t.morphDuration);
}

} // namespace cup::platform
