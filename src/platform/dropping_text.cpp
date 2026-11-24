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

// A line's sprite over a surface of its own, at the frame's top.
void makeLine(Compositor const& c, GraphicsDevice& device, DroppingText& dropping, Vec2 size)
{
    dropping.surface = device.createSurface(size);
    dropping.line = c.CreateSpriteVisual();
    dropping.line.Brush(c.CreateSurfaceBrush(dropping.surface));
    dropping.line.Size(toFloat2(size));
    dropping.visual.Children().InsertAtTop(dropping.line);
}

} // namespace

DroppingText createDroppingText(Compositor const& c, GraphicsDevice& device, const Text& text, Vec2 size, bool grey)
{
    DroppingText dropping{ .text = text, .grey = grey };
    dropping.visual = c.CreateContainerVisual();
    dropping.visual.Size(toFloat2(size));
    dropping.visual.Clip(c.CreateInsetClip()); // a line on its way in or out isn't seen past the frame
    makeLine(c, device, dropping, size);
    repaintDropping(device, dropping, size);
    return dropping;
}

bool dropTo(Compositor const& c, GraphicsDevice& device, DroppingText& dropping, const Text& text, Vec2 size)
{
    bool sameWords = text.value == dropping.text.value;
    dropping.text = text;
    if (sameWords) return repaintDropping(device, dropping, size); // its look changed, not its words
    const Tokens& t = tokens();
    SpriteVisual old = dropping.line;
    makeLine(c, device, dropping, size);
    bool drawn = repaintDropping(device, dropping, size);
    if (t.reducedMotion) { // swapped in place
        dropping.visual.Children().Remove(old);
        return drawn;
    }
    // The old line drops out below and is removed once gone; the new one
    // falls in from above, settling with a little bounce.
    auto batch = c.CreateScopedBatch(CompositionBatchTypes::Animation);
    slide(c, old, 0.0f, size.y, 1.0f, 0.0f, t.morphEase);
    batch.End();
    batch.Completed([old](auto&&, auto&&) {
        if (auto parent = old.Parent()) parent.Children().Remove(old);
    });
    slide(c, dropping.line, -size.y, 0.0f, 0.0f, 1.0f, t.settleEase);
    return drawn;
}

bool repaintDropping(GraphicsDevice& device, DroppingText& dropping, Vec2 size)
{
    return device.draw(dropping.surface, [&](ID2D1DeviceContext* dc) {
        auto draw = [&](ID2D1DeviceContext* target) { paintText(target, device.dwrite(), dropping.text, size); };
        if (dropping.grey) paintGrey(dc, draw); // a muted tab's (decision 072)
        else draw(dc);
    });
}

} // namespace cup::platform
