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

// Steps in one ripple: the path glides in a straight line from each to the next, so the
// swing dips a little between them; at twelve, by 3%, which isn't seen.
constexpr int kPhases = 12;
constexpr float kTurn = 6.2831853f;

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

CompositionPath pathThrough(ID2D1Factory* factory, const std::vector<Vec2>& points)
{
    return CompositionPath(winrt::make<GeometrySource>(lineThrough(factory, points)));
}

// All of one ripple, phase 0 to phase 0 again, over and over.
PathKeyFrameAnimation runOf(Compositor const& c, const WaveParts& wave)
{
    auto run = c.CreatePathKeyFrameAnimation();
    auto steady = c.CreateLinearEasingFunction();
    for (int i = 0; i <= kPhases; ++i) run.InsertKeyFrame(i / float(kPhases), wave.phases[i % kPhases], steady);
    run.Duration(std::max(tokens().progressWavePeriod, Ms{ 1 }));
    run.IterationBehavior(AnimationIterationBehavior::Forever);
    return run;
}

} // namespace

WaveParts createWave(Compositor const& c, ID2D1Factory* factory, Vec2 frame, CompositionColorBrush const& accent,
                     CompositionRoundedRectangleGeometry const& fill)
{
    const Tokens& t = tokens();
    float middle = frame.y / 2;
    if (!factory || wavePoints(frame.x, middle, 0).size() < 2) return {};
    WaveParts wave;
    try {
        // As long as the whole bar, from its head back: however far the bar is filled, it reaches the start.
        for (int i = 0; i < kPhases; ++i) wave.phases.push_back(pathThrough(factory, wavePoints(frame.x, middle, kTurn * i / kPhases)));
        wave.flat = pathThrough(factory, wavePoints(frame.x, middle, 0, 0));
        wave.geometry = c.CreatePathGeometry(wave.flat);
        wave.line = c.CreateSpriteShape(wave.geometry);
        wave.line.StrokeBrush(accent);
        wave.line.StrokeThickness(t.progressHeight);
        wave.line.StrokeLineJoin(CompositionStrokeLineJoin::Round);
        wave.line.StrokeStartCap(CompositionStrokeCap::Round);
        wave.line.StrokeEndCap(CompositionStrokeCap::Round);
        // Its head (the path's x = 0) is the fill's live end.
        auto head = c.CreateExpressionAnimation(L"Vector2(fill.Size.X, 0)");
        head.SetReferenceParameter(L"fill", fill);
        wave.line.StartAnimation(L"Offset", head);

        // Seen through a window with round ends, the swing's height: where the bar starts the
        // wave is cut along a curve, not straight down. (The window's other end is past the bar.)
        float swing = waveSwing();
        auto opening = c.CreateRoundedRectangleGeometry();
        opening.Size({ frame.x + swing, swing });
        opening.Offset({ 0.0f, (frame.y - swing) / 2 });
        opening.CornerRadius({ swing / 2, swing / 2 });
        wave.visual = c.CreateShapeVisual();
        wave.visual.Size(toFloat2(frame));
        wave.visual.Shapes().Append(wave.line);
        wave.visual.Clip(c.CreateGeometricClip(opening));
    } catch (winrt::hresult_error const& e) {
        logLine("progress: no wave, a plain bar (0x%08lx)", static_cast<unsigned long>(e.code()));
        return {};
    }
    wave.turn = std::make_shared<int>(0);
    return wave;
}

void ripple(Compositor const& c, WaveParts& wave, bool playing, bool atOnce)
{
    if (!wave.geometry || wave.rippling == playing) return;
    wave.rippling = playing;
    const Tokens& t = tokens();
    int turn = ++*wave.turn;
    bool runs = playing && !t.reducedMotion && t.progressWavePeriod > Ms{ 0 }; // reduced motion: it rises, and stays (decision 080)
    try {
        if (atOnce) {
            wave.geometry.StopAnimation(L"Path");
            wave.geometry.Path(playing ? wave.phases.front() : wave.flat);
            if (runs) wave.geometry.StartAnimation(L"Path", runOf(c, wave));
            return;
        }
        // From however it is now to flat, or to where a ripple starts.
        auto settle = c.CreatePathKeyFrameAnimation();
        settle.InsertKeyFrame(1.0f, playing ? wave.phases.front() : wave.flat,
                              c.CreateCubicBezierEasingFunction(toFloat2(t.morphEase.c1), toFloat2(t.morphEase.c2)));
        settle.Duration(std::max(t.progressWaveSettle, Ms{ 1 }));
        auto risen = c.CreateScopedBatch(CompositionBatchTypes::Animation);
        wave.geometry.StartAnimation(L"Path", settle);
        risen.End();
        if (!runs) return;
        // Risen, it runs: unless it was told otherwise meanwhile (paused again).
        risen.Completed([c, wave, turn](auto&&, auto&&) {
            if (*wave.turn != turn) return;
            try {
                wave.geometry.StartAnimation(L"Path", runOf(c, wave));
            } catch (winrt::hresult_error const& e) { // nothing may leave a handler (decision 158)
                logLine("progress: the wave couldn't run (0x%08lx)", static_cast<unsigned long>(e.code()));
            }
        });
    } catch (winrt::hresult_error const& e) {
        logLine("progress: the wave couldn't change (0x%08lx)", static_cast<unsigned long>(e.code()));
    }
}

} // namespace cup::platform
