#include "platform/element_parts.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;
using winrt::Windows::UI::Color;

// Composition rejects zero-length animations.
Ms atLeast1(Ms d) { return std::max(d, Ms{ 1 }); }

CompositionEasingFunction easeOf(Compositor const& c, Bezier curve)
{
    return c.CreateCubicBezierEasingFunction(toFloat2(curve.c1), toFloat2(curve.c2));
}

CompositionColorBrush white(Compositor const& c, std::uint8_t alpha) { return c.CreateColorBrush(Color{ alpha, 255, 255, 255 }); }

// Breathing waveform: symmetric ease-in-out, so a bar slows at its top and bottom.
constexpr Bezier kBreath{ { 0.45f, 0.0f }, { 0.55f, 1.0f } };
// Peak height per bar (fraction of the frame), repeating: uneven looks alive.
constexpr float kPeaks[] = { 0.6f, 1.0f, 0.78f };

} // namespace

void fadeIn(Compositor const& c, Visual const& visual)
{
    auto anim = c.CreateScalarKeyFrameAnimation();
    anim.InsertKeyFrame(0.0f, 0.0f);
    anim.InsertKeyFrame(1.0f, 1.0f, easeOf(c, tokens().morphEase));
    anim.Duration(atLeast1(tokens().contentFadeDuration));
    anim.DelayTime(tokens().contentFadeDelay); // let the pill start morphing first
    anim.DelayBehavior(AnimationDelayBehavior::SetInitialValueBeforeDelay);
    visual.StartAnimation(L"Opacity", anim);
}

void fadeTo(Compositor const& c, Visual const& visual, float opacity, Ms duration)
{
    auto anim = c.CreateScalarKeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, opacity, easeOf(c, tokens().morphEase));
    anim.Duration(atLeast1(duration));
    visual.StartAnimation(L"Opacity", anim);
}

void playScale(Compositor const& c, Visual const& visual, float scale, Ms duration, Bezier curve)
{
    auto anim = c.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { scale, scale, 1.0f }, easeOf(c, curve));
    anim.Duration(atLeast1(duration));
    visual.StartAnimation(L"Scale", anim); // replaces whatever was animating Scale
}

// --- Progress ----------------------------------------------------------------

float progressOverhang() { return tokens().progressKnobSize / 2; }

ProgressParts createProgress(Compositor const& c, Vec2 frame)
{
    const Tokens& t = tokens();
    float h = t.progressHeight, x = progressOverhang(), y = (frame.y - h) / 2;
    auto bar = [&](float width, std::uint8_t alpha) {
        auto geometry = c.CreateRoundedRectangleGeometry();
        geometry.Size({ width, h });
        geometry.Offset({ x, y });
        geometry.CornerRadius({ h / 2, h / 2 });
        auto shape = c.CreateSpriteShape(geometry);
        shape.FillBrush(white(c, alpha));
        return std::pair{ geometry, shape };
    };

    ProgressParts parts;
    parts.visual = c.CreateShapeVisual();
    parts.visual.Shapes().Append(bar(frame.x, 60).second);
    auto [fill, fillShape] = bar(0, 230);
    parts.visual.Shapes().Append(fillShape);
    parts.fill = fill;

    // The knob is bound to the fill's live (animating) end, so it tracks
    // playback on the compositor with no app code per frame.
    float r = t.progressKnobSize / 2;
    auto knob = c.CreateEllipseGeometry();
    knob.Radius({ r, r });
    auto center = c.CreateExpressionAnimation(L"Vector2(fill.Offset.X + fill.Size.X, fill.Offset.Y + fill.Size.Y / 2)");
    center.SetReferenceParameter(L"fill", fill);
    knob.StartAnimation(L"Center", center);
    parts.knob = c.CreateSpriteShape(knob);
    parts.knob.FillBrush(white(c, 255));
    parts.knob.StartAnimation(L"CenterPoint", center); // grows from its own middle
    parts.knob.Scale({ 0.0f, 0.0f });
    parts.visual.Shapes().Append(parts.knob);
    return parts;
}

void armProgress(Compositor const& c, CompositionRoundedRectangleGeometry const& fill, const Timeline& timeline,
                 float width, Ms now)
{
    float height = tokens().progressHeight;
    float from = timeline.fractionAt(now) * width;
    holdProgress(fill, from);
    if (!timeline.playing || timeline.rate <= 0 || timeline.duration <= Ms{ 0 }) return;

    Ms left = timeline.duration - timeline.positionAt(now);
    Ms wall{ static_cast<long long>(static_cast<double>(left.count()) / timeline.rate) };
    if (wall <= Ms{ 0 }) return;
    auto anim = c.CreateVector2KeyFrameAnimation();
    anim.InsertKeyFrame(0.0f, { from, height });
    anim.InsertKeyFrame(1.0f, { width, height }, c.CreateLinearEasingFunction());
    anim.Duration(std::min(wall, Ms{ 24LL * 3600 * 1000 })); // Composition caps durations
    fill.StartAnimation(L"Size", anim);
}

void holdProgress(CompositionRoundedRectangleGeometry const& fill, float fillWidth)
{
    fill.StopAnimation(L"Size");
    fill.Size({ fillWidth, tokens().progressHeight });
}

void showKnob(Compositor const& c, CompositionSpriteShape const& knob, bool visible)
{
    float s = visible ? 1.0f : 0.0f;
    auto anim = c.CreateVector2KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { s, s }, easeOf(c, tokens().morphEase));
    anim.Duration(atLeast1(tokens().hoverFadeDuration));
    knob.StartAnimation(L"Scale", anim);
}

// --- Waveform ----------------------------------------------------------------

WaveformParts createWaveform(Compositor const& c, Vec2 frame)
{
    const Tokens& t = tokens();
    size_t count = std::max<size_t>(t.waveformBarCount, 1);
    float w = t.waveformBarWidth, gap = t.waveformBarGap;
    float x = (frame.x - (static_cast<float>(count) * w + static_cast<float>(count - 1) * gap)) / 2;

    WaveformParts parts;
    parts.visual = c.CreateShapeVisual();
    for (size_t i = 0; i < count; ++i) {
        auto bar = c.CreateRoundedRectangleGeometry();
        bar.CornerRadius({ w / 2, w / 2 });
        bar.Size({ w, w }); // a dot until it plays
        // Stays vertically centred at whatever height it's animating through.
        auto offset = c.CreateExpressionAnimation(L"Vector2(left, (height - bar.Size.Y) / 2)");
        offset.SetReferenceParameter(L"bar", bar);
        offset.SetScalarParameter(L"left", x + static_cast<float>(i) * (w + gap));
        offset.SetScalarParameter(L"height", frame.y);
        bar.StartAnimation(L"Offset", offset);
        auto shape = c.CreateSpriteShape(bar);
        shape.FillBrush(white(c, 230));
        parts.visual.Shapes().Append(shape);
        parts.bars.push_back(bar);
    }
    return parts;
}

void animateWaveform(Compositor const& c, const WaveformParts& parts, float height, bool active)
{
    const Tokens& t = tokens();
    float w = t.waveformBarWidth;
    for (size_t i = 0; i < parts.bars.size(); ++i) {
        auto anim = c.CreateVector2KeyFrameAnimation();
        if (!active) {
            // Paused: each bar settles from wherever it is into a still dot.
            anim.InsertKeyFrame(1.0f, { w, w }, easeOf(c, t.morphEase));
            anim.Duration(atLeast1(t.contentFadeDuration));
        } else {
            float peak = std::max(w, height * kPeaks[i % std::size(kPeaks)]);
            anim.InsertKeyFrame(0.0f, { w, w });
            anim.InsertKeyFrame(0.5f, { w, peak }, easeOf(c, kBreath));
            anim.InsertKeyFrame(1.0f, { w, w }, easeOf(c, kBreath));
            // Neighbours run slightly slower, so the bars drift in and out of step.
            anim.Duration(atLeast1(t.waveformPeriod + t.waveformPeriod * static_cast<long long>(i) / 6));
            anim.IterationBehavior(AnimationIterationBehavior::Forever);
        }
        parts.bars[i].StartAnimation(L"Size", anim);
    }
}

// --- Button ------------------------------------------------------------------

ButtonParts createButton(Compositor const& c, Vec2 frame, float cornerRadius, CompositionDrawingSurface const& glyph)
{
    auto shape = c.CreateRoundedRectangleGeometry();
    shape.Size(toFloat2(frame));
    shape.CornerRadius({ cornerRadius, cornerRadius });
    auto fill = c.CreateSpriteShape(shape);
    fill.FillBrush(white(c, 40));

    ButtonParts parts;
    parts.highlight = c.CreateShapeVisual();
    parts.highlight.Size(toFloat2(frame));
    parts.highlight.Shapes().Append(fill);
    parts.highlight.Opacity(0.0f);

    auto glyphVisual = c.CreateSpriteVisual();
    glyphVisual.Brush(c.CreateSurfaceBrush(glyph));
    glyphVisual.Size(toFloat2(frame));

    parts.visual = c.CreateContainerVisual();
    parts.visual.Children().InsertAtTop(parts.highlight);
    parts.visual.Children().InsertAtTop(glyphVisual);
    return parts;
}

// --- Swimming text -----------------------------------------------------------

CompositionBrush createMarqueeBrush(Compositor const& c, CompositionDrawingSurface const& surface, Vec2 visualSize,
                                    float fadeWidth, float travel)
{
    const Tokens& t = tokens();
    auto text = c.CreateSurfaceBrush(surface);
    text.Stretch(CompositionStretch::None); // natural size, pinned left: the visual is a window onto it
    text.HorizontalAlignmentRatio(0.0f);
    text.VerticalAlignmentRatio(0.0f);

    Ms scroll{ static_cast<long long>(travel / std::max(t.marqueeSpeed, 1.0f) * 1000.0f) };
    Ms total = atLeast1(t.marqueePause + scroll);
    float hold = static_cast<float>(t.marqueePause.count()) / static_cast<float>(total.count());
    auto anim = c.CreateVector2KeyFrameAnimation();
    anim.InsertKeyFrame(0.0f, { 0.0f, 0.0f });
    if (hold > 0.0f && hold < 1.0f) anim.InsertKeyFrame(hold, { 0.0f, 0.0f });
    anim.InsertKeyFrame(1.0f, { -travel, 0.0f }, c.CreateLinearEasingFunction());
    anim.Duration(total);
    anim.IterationBehavior(AnimationIterationBehavior::Forever);
    text.StartAnimation(L"Offset", anim);

    // Soft edges: text dissolves where it enters and leaves instead of being cut.
    float fade = std::clamp(fadeWidth / std::max(visualSize.x, 1.0f), 0.0f, 0.5f);
    auto mask = c.CreateLinearGradientBrush();
    mask.StartPoint({ 0.0f, 0.0f });
    mask.EndPoint({ 1.0f, 0.0f });
    const std::pair<float, std::uint8_t> stops[] = { { 0.0f, 0 }, { fade, 255 }, { 1.0f - fade, 255 }, { 1.0f, 0 } };
    for (auto [offset, alpha] : stops) mask.ColorStops().Append(c.CreateColorGradientStop(offset, Color{ alpha, 255, 255, 255 }));

    auto brush = c.CreateMaskBrush();
    brush.Source(text);
    brush.Mask(mask);
    return brush;
}

} // namespace cup::platform
