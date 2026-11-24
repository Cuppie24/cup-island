#include "platform/dropping_text.h"

#include "core/tokens.h"
#include "platform/element_painting.h"

#include <algorithm>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Slides it from `from` to `to` down its frame, fading between the two opacities.
void slide(Compositor const& c, Visual const& visual, float from, float to, float fadeFrom, float fadeTo, Bezier curve)
{
    Ms duration = std::max(tokens().lyricDropDuration, Ms{ 1 }); // Composition rejects zero-length animations
    auto offset = c.CreateScalarKeyFrameAnimation();
    offset.InsertKeyFrame(0.0f, from);
    offset.InsertKeyFrame(1.0f, to, c.CreateCubicBezierEasingFunction(toFloat2(curve.c1), toFloat2(curve.c2)));
    offset.Duration(duration);
    visual.StartAnimation(L"Offset.Y", offset);
    auto opacity = c.CreateScalarKeyFrameAnimation();
    opacity.InsertKeyFrame(0.0f, fadeFrom);
    opacity.InsertKeyFrame(1.0f, fadeTo, c.CreateCubicBezierEasingFunction(toFloat2(tokens().morphEase.c1), toFloat2(tokens().morphEase.c2)));
    opacity.Duration(duration);
    visual.StartAnimation(L"Opacity", opacity);
}

// A line's sprite over a surface of its own, at the frame's top. One too long for the frame,
// with time to be read (Text::scrollUntil): its surface as wide as its words, which then
// scroll through the frame once, still at the start and at the end, done by that moment.
void makeLine(Compositor const& c, GraphicsDevice& device, DroppingText& dropping, Vec2 size, Ms now)
{
    const Tokens& t = tokens();
    const Text& text = dropping.text;
    Ms left = text.scrollUntil ? *text.scrollUntil - now : Ms{ 0 };
    float width = left >= t.lyricScrollLeast && !t.reducedMotion ? measureText(device.dwrite(), text) + 2 : 0; // + 2: a slant's overhang
    dropping.lineWidth = width > size.x ? width : 0;
    Vec2 lineSize{ dropping.lineWidth > 0 ? dropping.lineWidth : size.x, size.y };
    dropping.surface = device.createSurface(lineSize);
    dropping.line = c.CreateSpriteVisual();
    dropping.line.Brush(c.CreateSurfaceBrush(dropping.surface));
    dropping.line.Size(toFloat2(lineSize));
    dropping.visual.Children().InsertAtTop(dropping.line);
    if (dropping.lineWidth <= 0) return;
    float holdStart = std::clamp(t.lyricScrollHoldStart, 0.0f, 0.9f), holdEnd = std::clamp(t.lyricScrollHoldEnd, 0.0f, 0.9f - holdStart);
    auto scroll = c.CreateScalarKeyFrameAnimation();
    auto steady = c.CreateLinearEasingFunction(); // words pass at one speed: easier to read than easing
    scroll.InsertKeyFrame(0.0f, 0.0f);
    scroll.InsertKeyFrame(holdStart, 0.0f, steady);
    scroll.InsertKeyFrame(1.0f - holdEnd, size.x - dropping.lineWidth, steady);
    scroll.InsertKeyFrame(1.0f, size.x - dropping.lineWidth, steady);
    scroll.Duration(left);
    dropping.line.StartAnimation(L"Offset.X", scroll);
}

} // namespace

DroppingText createDroppingText(Compositor const& c, GraphicsDevice& device, const Text& text, Vec2 size, bool grey, Ms now)
{
    DroppingText dropping{ .text = text, .grey = grey };
    dropping.visual = c.CreateContainerVisual();
    dropping.visual.Size(toFloat2(size));
    dropping.visual.Clip(c.CreateInsetClip()); // a line on its way in or out isn't seen past the frame
    if (!text.actionId.empty()) { // under every line, hidden until the pointer is on it
        auto shape = c.CreateRoundedRectangleGeometry();
        shape.Size(toFloat2(size));
        shape.CornerRadius({ size.y / 2, size.y / 2 });
        dropping.hoverBrush = c.CreateColorBrush(toUiColor(tokens().buttonHoverColor));
        auto fill = c.CreateSpriteShape(shape);
        fill.FillBrush(dropping.hoverBrush);
        dropping.hover = c.CreateShapeVisual();
        dropping.hover.Size(toFloat2(size));
        dropping.hover.Shapes().Append(fill);
        dropping.hover.Opacity(0.0f);
        dropping.visual.Children().InsertAtBottom(dropping.hover);
    }
    makeLine(c, device, dropping, size, now);
    repaintDropping(device, dropping, size);
    return dropping;
}

bool dropTo(Compositor const& c, GraphicsDevice& device, DroppingText& dropping, const Text& text, Vec2 size, Ms now)
{
    bool sameWords = text.value == dropping.text.value;
    dropping.text = text;
    if (sameWords) return repaintDropping(device, dropping, size); // its look changed, not its words
    const Tokens& t = tokens();
    SpriteVisual old = dropping.line;
    makeLine(c, device, dropping, size, now);
    bool drawn = repaintDropping(device, dropping, size);
    if (t.reducedMotion) { // swapped in place
        dropping.visual.Children().Remove(old);
        return drawn;
    }
    // The old line drops out below and is removed once gone; the new one
    // falls in from above, settling with a little bounce.
    // Rising (Text::rises): the other way, the old one out at the top, the new one up from below.
    float way = text.rises ? -1.0f : 1.0f;
    auto batch = c.CreateScopedBatch(CompositionBatchTypes::Animation);
    slide(c, old, 0.0f, way * size.y, 1.0f, 0.0f, t.morphEase);
    batch.End();
    batch.Completed([old](auto&&, auto&&) {
        if (auto parent = old.Parent()) parent.Children().Remove(old);
    });
    slide(c, dropping.line, -way * size.y, 0.0f, 0.0f, 1.0f, text.rises ? t.morphEase : t.settleEase); // rising, it doesn't bounce: rows scrolling together would jitter
    return drawn;
}

bool repaintDropping(GraphicsDevice& device, DroppingText& dropping, Vec2 size)
{
    return device.draw(dropping.surface, [&](ID2D1DeviceContext* dc) {
        // A scrolling line: all of it, from its surface's start (no centring, no "…").
        Text shown = dropping.text;
        if (dropping.lineWidth > 0) shown.centred = shown.alignEnd = false;
        Vec2 lineSize{ dropping.lineWidth > 0 ? dropping.lineWidth : size.x, size.y };
        auto draw = [&](ID2D1DeviceContext* target) { paintText(target, device.dwrite(), shown, lineSize); };
        if (dropping.grey) paintGrey(dc, draw); // a muted tab's (decision 072)
        else draw(dc);
    });
}

} // namespace cup::platform
