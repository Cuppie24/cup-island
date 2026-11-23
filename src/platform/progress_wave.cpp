#include "platform/progress_wave.h"

#include "core/item_metrics.h"
#include "core/tokens.h"
#include "platform/log.h"

#include <windows.graphics.interop.h>
#include <winrt/Windows.Graphics.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace cup::platform {
namespace {

using namespace winrt::Windows::UI::Composition;

// Lying flat: the wave squashed to its middle line. Not 0: a shape scaled to nothing may not be drawn.
constexpr float kFlat = 0.02f;

// How Composition takes a Direct2D geometry: an object that hands it over.
struct GeometrySource : winrt::implements<GeometrySource, winrt::Windows::Graphics::IGeometrySource2D,
                                          ABI::Windows::Graphics::IGeometrySource2DInterop> {
    explicit GeometrySource(winrt::com_ptr<ID2D1Geometry> geometry) : geometry_(std::move(geometry)) {}
    HRESULT __stdcall GetGeometry(ID2D1Geometry** value) noexcept override
    {
        geometry_.copy_to(value);
        return S_OK;
    }
    HRESULT __stdcall TryGetGeometryUsingFactory(ID2D1Factory*, ID2D1Geometry** value) noexcept override
    {
        *value = nullptr;
        return E_NOTIMPL;
    }

private:
    winrt::com_ptr<ID2D1Geometry> geometry_;
};

winrt::com_ptr<ID2D1Geometry> lineThrough(ID2D1Factory* factory, const std::vector<Vec2>& points)
{
    winrt::com_ptr<ID2D1PathGeometry> path;
    winrt::check_hresult(factory->CreatePathGeometry(path.put()));
    winrt::com_ptr<ID2D1GeometrySink> sink;
    winrt::check_hresult(path->Open(sink.put()));
    sink->BeginFigure(D2D1::Point2F(points.front().x, points.front().y), D2D1_FIGURE_BEGIN_HOLLOW);
    for (std::size_t i = 1; i < points.size(); ++i) sink->AddLine(D2D1::Point2F(points[i].x, points[i].y));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    winrt::check_hresult(sink->Close());
    return path;
}

} // namespace

WaveParts createWave(Compositor const& c, ID2D1Factory* factory, Vec2 frame, CompositionColorBrush const& accent,
                     CompositionRoundedRectangleGeometry const& fill)
{
    const Tokens& t = tokens();
    // A wave longer than the bar: it slides a wave's length and starts over, unseen.
    // From x = 0: it slides to the left, so the bar's end is always covered.
    std::vector<Vec2> points = wavePoints(frame.x + t.progressWaveLength, frame.y / 2);
    if (!factory || points.size() < 2) return {};
    WaveParts wave;
    try {
        CompositionPath path(winrt::make<GeometrySource>(lineThrough(factory, points)));
        wave.line = c.CreateSpriteShape(c.CreatePathGeometry(path));
    } catch (winrt::hresult_error const& e) {
        logLine("progress: no wave, a plain bar (0x%08lx)", static_cast<unsigned long>(e.code()));
        return {};
    }
    wave.line.StrokeBrush(accent);
    wave.line.StrokeThickness(t.progressHeight);
    wave.line.StrokeLineJoin(CompositionStrokeLineJoin::Round);
    wave.line.StrokeStartCap(CompositionStrokeCap::Round);
    wave.line.StrokeEndCap(CompositionStrokeCap::Round);
    wave.line.IsStrokeNonScaling(true); // squashed flat, it's still the bar's thickness
    wave.line.CenterPoint({ 0.0f, frame.y / 2 });
    wave.line.Scale({ 1.0f, kFlat });

    // Seen through a window with round ends, the swing's height: where the wave
    // starts it's cut along a curve, not straight down. (Its other end is past the bar.)
    float swing = waveSwing();
    auto opening = c.CreateRoundedRectangleGeometry();
    opening.Size({ frame.x + swing, swing });
    opening.Offset({ 0.0f, (frame.y - swing) / 2 });
    opening.CornerRadius({ swing / 2, swing / 2 });
    auto window = c.CreateShapeVisual();
    window.Size(toFloat2(frame));
    window.Shapes().Append(wave.line);
    window.Clip(c.CreateGeometricClip(opening));

    wave.visual = c.CreateContainerVisual();
    wave.visual.Size(toFloat2(frame));
    wave.visual.Children().InsertAtTop(window);
    // Shown as far as the bar is filled: the clip's right edge is the fill's live end.
    auto clip = c.CreateInsetClip();
    auto inset = c.CreateExpressionAnimation(L"Max(width - fill.Size.X, 0)");
    inset.SetReferenceParameter(L"fill", fill);
    inset.SetScalarParameter(L"width", frame.x);
    clip.StartAnimation(L"RightInset", inset);
    wave.visual.Clip(clip);
    return wave;
}

void ripple(Compositor const& c, WaveParts& wave, bool playing, bool atOnce)
{
    if (!wave.line || wave.rippling == playing) return;
    wave.rippling = playing;
    const Tokens& t = tokens();
    wfn::float2 scale{ 1.0f, playing ? 1.0f : kFlat };
    if (atOnce) {
        wave.line.Scale(scale);
    } else {
        auto settle = c.CreateVector2KeyFrameAnimation();
        settle.InsertKeyFrame(1.0f, scale, c.CreateCubicBezierEasingFunction(toFloat2(t.morphEase.c1), toFloat2(t.morphEase.c2)));
        settle.Duration(std::max(t.progressWaveSettle, Ms{ 1 }));
        wave.line.StartAnimation(L"Scale", settle);
    }
    // Still when paused (it's flat: nothing to see move), and for reduced motion (decision 080).
    if (!playing || t.reducedMotion || t.progressWavePeriod <= Ms{ 0 }) {
        wave.line.StopAnimation(L"Offset");
        return;
    }
    // One wave's length back from the head, over and over: the head leads and the
    // ripples trail behind it, as behind something swimming. Both ends given (decision 008).
    auto run = c.CreateVector2KeyFrameAnimation();
    run.InsertKeyFrame(0.0f, { 0.0f, 0.0f });
    run.InsertKeyFrame(1.0f, { -t.progressWaveLength, 0.0f }, c.CreateLinearEasingFunction());
    run.Duration(t.progressWavePeriod);
    run.IterationBehavior(AnimationIterationBehavior::Forever);
    wave.line.StartAnimation(L"Offset", run);
}

} // namespace cup::platform
