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

// The share of the animation that is the rise (flat to the full swing); the rest is one ripple.
constexpr float kRise = 0.4f;
// Steps in rising or lying down: the swing eases between them, the ripples run on through them.
constexpr int kSteps = 10;

// 0..1, starting and ending with no speed and no jolt (smootherstep): how a swing grows or dies away.
float eased(float u)
{
    u = std::clamp(u, 0.0f, 1.0f);
    return u * u * u * (u * (u * 6 - 15) + 10);
}

// How far the ripples run, in turns, while the wave rises or lies down: at their own speed.
float turnsWhileSettling()
{
    const Tokens& t = tokens();
    if (t.reducedMotion || t.progressWavePeriod <= Ms{ 0 }) return 0;
    return static_cast<float>(t.progressWaveSettle.count()) / static_cast<float>(t.progressWavePeriod.count());
}

// Rising (wave.rising: flat, the swing growing as the ripples run), then all of one ripple from
// where rising left it and back there. Never played by its own time: shown() says how far along it is.
PathKeyFrameAnimation risingThenRunning(Compositor const& c, const WaveParts& wave)
{
    auto anim = c.CreatePathKeyFrameAnimation();
    auto steady = c.CreateLinearEasingFunction();
    anim.InsertKeyFrame(0.0f, wave.rising.front());
    for (int i = 1; i < kSteps; ++i) anim.InsertKeyFrame(kRise * i / float(kSteps), wave.rising[i], steady);
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
        // The ripple starts where rising leaves the line: the ripples ran on while it rose.
        float risen = kTurn * turnsWhileSettling();
        for (int i = 0; i < kSteps; ++i)
            wave.rising.push_back(pathThrough(factory, wavePoints(frame.x, middle, risen * i / kSteps, eased(i / float(kSteps)))));
        for (int i = 0; i < kPhases; ++i) wave.phases.push_back(pathThrough(factory, wavePoints(frame.x, middle, risen + kTurn * i / kPhases)));
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
        // Steadily: the easing is in the steps' swings, and the ripples run at one speed through them.
        auto up = c.CreateScalarKeyFrameAnimation(); // both ends given (decision 008)
        up.InsertKeyFrame(0.0f, 0.0f);
        up.InsertKeyFrame(1.0f, 1.0f, c.CreateLinearEasingFunction());
        up.Duration(wave.rise);
        wave.state.StartAnimation(L"Rise", up);
    }
    if (wave.runs) {
        // Starts at 0 as the rise ends: the ripple's first phase is where rising left the line.
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
    // The line as it is now, worked out as shown() does: how far it had risen, and which phase.
    auto since = std::chrono::duration_cast<Ms>(std::chrono::steady_clock::now() - wave.roseAt);
    float settling = kTurn * turnsWhileSettling();
    float swing = 1, phase = settling;
    if (since < wave.rise) {
        float risen = static_cast<float>(since.count()) / static_cast<float>(wave.rise.count());
        swing = eased(risen);
        phase = settling * risen;
    } else if (wave.runs) {
        float clock = fraction(static_cast<float>((since - wave.rise).count()) / static_cast<float>(t.progressWavePeriod.count()));
        phase += kTurn * fraction(clock - (head - wave.anchor) / std::max(t.progressWaveLength, 1.0f));
    } else {
        phase += kTurn * fraction(-(head - wave.anchor) / std::max(t.progressWaveLength, 1.0f));
    }
    // From there to flat, both ends given (decision 008): the ripples run on at their speed as
    // the swing dies away, as they did while it rose.
    auto down = c.CreatePathKeyFrameAnimation();
    auto steady = c.CreateLinearEasingFunction();
    float middle = wave.frame.y / 2;
    for (int i = 0; i < kSteps; ++i) {
        float u = i / float(kSteps);
        auto path = pathThrough(wave.factory, wavePoints(wave.frame.x, middle, phase + (wave.runs ? settling * u : 0.0f), swing * (1 - eased(u))));
        if (i == 0) down.InsertKeyFrame(0.0f, path);
        else down.InsertKeyFrame(u, path, steady);
    }
    down.InsertKeyFrame(1.0f, wave.flat, steady);
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
