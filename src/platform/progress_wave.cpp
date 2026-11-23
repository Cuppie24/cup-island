#include "platform/progress_wave.h"

#include "core/item_metrics.h"
#include "core/tokens.h"
#include "platform/log.h"

#include <windows.graphics.interop.h>
#include <winrt/Windows.Graphics.h>

#include <algorithm>
#include <chrono>
#include <cmath>
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

// The share of the animation that is the rise (flat to the first phase); the rest is one ripple.
constexpr float kRise = 0.2f;

// Flat, the first phase, then all of one ripple back to the first phase. Never played by its
// own time: shown() says how far along it is.
PathKeyFrameAnimation risingThenRunning(Compositor const& c, const WaveParts& wave)
{
    auto anim = c.CreatePathKeyFrameAnimation();
    auto steady = c.CreateLinearEasingFunction();
    anim.InsertKeyFrame(0.0f, wave.flat);
    for (int i = 0; i <= kPhases; ++i) anim.InsertKeyFrame(kRise + (1 - kRise) * i / float(kPhases), wave.phases[i % kPhases], steady);
    anim.Duration(std::chrono::seconds(1));
    return anim;
}

// How far along risingThenRunning the line is: rising, by how far it has risen; risen, by the
// clock, less the head's distance from where it rose in waves. A head dragged a wave's length
// to the right shows the phase of a ripple earlier: on the bar, the ripples haven't moved.
ExpressionAnimation shown(Compositor const& c, const WaveParts& wave)
{
    auto at = c.CreateExpressionAnimation(
        L"wave.Rise < 1 ? rise * wave.Rise : rise + (1 - rise) * ((wave.Clock - (fill.Size.X - wave.Anchor) / length) - "
        L"Floor(wave.Clock - (fill.Size.X - wave.Anchor) / length))");
    at.SetReferenceParameter(L"wave", wave.state);
    at.SetReferenceParameter(L"fill", wave.fill);
    at.SetScalarParameter(L"rise", kRise);
    at.SetScalarParameter(L"length", std::max(tokens().progressWaveLength, 1.0f));
    return at;
}

float fraction(float value) { return value - std::floor(value); }

CompositionEasingFunction gentle(Compositor const& c)
{
    return c.CreateCubicBezierEasingFunction({ 0.4f, 0.0f }, { 0.2f, 1.0f }); // in and out: a swing growing or dying away
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

        wave.state = c.CreatePropertySet();
        wave.state.InsertScalar(L"Rise", 0.0f);
        wave.state.InsertScalar(L"Clock", 0.0f);
        wave.state.InsertScalar(L"Anchor", 0.0f);
    } catch (winrt::hresult_error const& e) {
        logLine("progress: no wave, a plain bar (0x%08lx %ls)", static_cast<unsigned long>(e.code()), e.message().c_str());
        return {};
    }
    wave.fill = fill;
    wave.factory = factory;
    wave.frame = frame;
    return wave;
}

namespace {

void rise(Compositor const& c, WaveParts& wave, float head, bool atOnce)
{
    const Tokens& t = tokens();
    wave.rise = atOnce ? Ms{ 0 } : std::max(t.progressWaveSettle, Ms{ 1 });
    wave.roseAt = std::chrono::steady_clock::now();
    wave.runs = !t.reducedMotion && t.progressWavePeriod > Ms{ 0 }; // reduced motion: it rises, and stays (decision 080)
    wave.anchor = head;
    wave.state.StopAnimation(L"Rise");
    wave.state.StopAnimation(L"Clock");
    wave.state.InsertScalar(L"Anchor", head);
    wave.state.InsertScalar(L"Clock", 0.0f);
    wave.state.InsertScalar(L"Rise", atOnce ? 1.0f : 0.0f);
    if (!atOnce) {
        auto up = c.CreateScalarKeyFrameAnimation(); // both ends given (decision 008)
        up.InsertKeyFrame(0.0f, 0.0f);
        up.InsertKeyFrame(1.0f, 1.0f, gentle(c));
        up.Duration(wave.rise);
        wave.state.StartAnimation(L"Rise", up);
    }
    if (wave.runs) {
        // Starts at 0 as the rise ends: the first phase, where the rise left the line.
        auto clock = c.CreateScalarKeyFrameAnimation();
        clock.InsertKeyFrame(0.0f, 0.0f);
        clock.InsertKeyFrame(1.0f, 1.0f, c.CreateLinearEasingFunction());
        clock.Duration(t.progressWavePeriod);
        clock.IterationBehavior(AnimationIterationBehavior::Forever);
        if (!atOnce) {
            clock.DelayTime(wave.rise);
            clock.DelayBehavior(AnimationDelayBehavior::SetInitialValueBeforeDelay);
        }
        wave.state.StartAnimation(L"Clock", clock);
    }
    // The path's animation, held still, its progress the expression's.
    wave.geometry.StartAnimation(L"Path", risingThenRunning(c, wave));
    auto controller = wave.geometry.TryGetAnimationController(L"Path");
    if (!controller) {
        logLine("progress: the wave's animation can't be driven; it stays flat");
        wave.geometry.StopAnimation(L"Path");
        return;
    }
    controller.Pause();
    controller.StartAnimation(L"Progress", shown(c, wave));
}

void lieDown(Compositor const& c, WaveParts& wave, float head, bool atOnce)
{
    const Tokens& t = tokens();
    wave.state.StopAnimation(L"Rise");
    wave.state.StopAnimation(L"Clock");
    if (atOnce || !wave.rippling) {
        wave.geometry.StopAnimation(L"Path");
        wave.geometry.Path(wave.flat);
        return;
    }
    // The line as it is now, worked out as shown() does: how far it had risen, or which phase.
    auto since = std::chrono::duration_cast<Ms>(std::chrono::steady_clock::now() - wave.roseAt);
    float swing = 1, phase = 0;
    if (since < wave.rise) {
        swing = static_cast<float>(since.count()) / static_cast<float>(wave.rise.count());
    } else {
        float clock = wave.runs ? fraction(static_cast<float>((since - wave.rise).count()) / static_cast<float>(t.progressWavePeriod.count())) : 0.0f;
        phase = kTurn * fraction(clock - (head - wave.anchor) / std::max(t.progressWaveLength, 1.0f));
    }
    auto down = c.CreatePathKeyFrameAnimation(); // both ends given (decision 008): from there to flat
    down.InsertKeyFrame(0.0f, pathThrough(wave.factory, wavePoints(wave.frame.x, wave.frame.y / 2, phase, swing)));
    down.InsertKeyFrame(1.0f, wave.flat, gentle(c));
    down.Duration(std::max(t.progressWaveSettle, Ms{ 1 }));
    wave.geometry.StartAnimation(L"Path", down);
}

} // namespace

void ripple(Compositor const& c, WaveParts& wave, bool playing, float head, bool atOnce)
{
    if (!wave.geometry || wave.rippling == playing) return;
    try {
        if (playing) rise(c, wave, head, atOnce);
        else lieDown(c, wave, head, atOnce);
    } catch (winrt::hresult_error const& e) {
        logLine("progress: the wave couldn't %s (0x%08lx %ls)", playing ? "rise" : "lie down", static_cast<unsigned long>(e.code()),
                e.message().c_str());
    }
    wave.rippling = playing;
}

} // namespace cup::platform
