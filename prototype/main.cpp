// Cup Island - minimal animation showoff prototype
//
// Demonstrates the core technique required by section 5 of the technical
// requirements doc: a borderless, topmost, compositor-owned window whose
// pill shape expands/contracts/morphs using Windows.UI.Composition implicit
// and expression animations only - no manual per-frame redraws, no timer
// driving visual state once idle.
//
// Windowing follows section 9.1's two-layer split:
//   - Render layer: one large WS_EX_LAYERED | WS_EX_TRANSPARENT window,
//     sized once to the pill's maximum footprint and never resized (an
//     HWND resize mid-animation is exactly the lag section 5 rules out).
//     Every visual lives here. WS_EX_TRANSPARENT excludes it from
//     hit-testing altogether, so a click anywhere on it - including the
//     empty area around the pill - goes to whatever is underneath.
//   - Interaction layer: a small window that draws nothing and is snapped
//     (SetWindowPos, cheap) to the pill's current bounds, so it receives
//     exactly the clicks that land on the visible pill. During a morph it is
//     re-snapped once per compositor frame; see SyncInteractionLayer.
//
// Interaction: pressing the pill scales it down slightly and releasing
// springs it back (tactile click feedback - there's no button chrome here
// to otherwise signal it's clickable); releasing on the pill toggles
// collapsed/expanded. Clicking anywhere else - another window, the desktop,
// or just empty space the render layer happens to cover - shrinks it back
// down if it's expanded. Esc to quit.
//
// Deliberately not animated: a hover reaction. The real Dynamic Island is
// touch-only and has no hover concept; a mouse-proximity pulse would just
// be noise for something sitting at the top of the screen where the cursor
// passes through constantly during unrelated work. The animation worth
// building next is event-driven instead - a brief attention pulse when a
// real data source (section 6-7) actually has something new to show - but
// that needs those providers wired in first; faking it off mouse position
// would be theater, not signal.
//
// Deliberately out of scope here (these are later milestones in the
// requirements doc, not part of a minimal animation showoff):
//   - WinUI 3 / XAML shell (this uses Win32 + Composition directly)
//   - Real media/clipboard/battery data (section 6-7) - content is a static
//     placeholder layout (icon + text bars + progress track)
//   - Multi-monitor/DPI correctness, theme following, acrylic backdrop blur
//   - MSIX packaging
//
// Build: run build.ps1 in this folder (PowerShell). Output: bin\CupIsland.exe

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <dcomp.h>
#include <dispatcherqueue.h>
#include <windows.ui.composition.interop.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

#pragma comment(lib, "WindowsApp.lib")
#pragma comment(lib, "CoreMessaging.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "dcomp.lib")

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Composition::Desktop;
using namespace std::chrono_literals;

namespace {

constexpr wchar_t kRenderClassName[] = L"CupIslandRenderWindow";
constexpr wchar_t kInputClassName[] = L"CupIslandInputWindow";

// The render layer is sized once to the largest footprint the pill can
// ever have (expanded size plus room for the press-release overshoot) and
// is never resized; the pill grows/shrinks inside it, hanging from the
// window's (and so the screen's) top edge.
constexpr int kRenderWidth = 420;
constexpr int kRenderHeight = 150;

constexpr float2 kCollapsedSize{ 160.0f, 28.0f };
constexpr float2 kExpandedSize{ 380.0f, 108.0f };

// Morph timing, shared by the implicit animations (BuildScene) and the
// app-side mirror that keeps the interaction layer in step with them.
// Cubic-bezier ease-out, morph budget 250-350ms per section 5.
constexpr auto kMorphDuration = 300ms;
constexpr float2 kMorphEaseControl1{ 0.16f, 1.0f };
constexpr float2 kMorphEaseControl2{ 0.30f, 1.0f };

// Posted to the interaction window by CompositorTickThread, once per
// composed frame while a morph is in flight.
constexpr UINT WM_APP_COMPOSITOR_TICK = WM_APP + 1;

HWND g_renderHwnd{};
HWND g_inputHwnd{};
int g_renderLeft = 0;   // screen x of the render layer; it always sits at y = 0
Compositor g_compositor{ nullptr };
ShapeVisual g_pill{ nullptr };
ContainerVisual g_content{ nullptr };
bool g_expanded = false;
bool g_pressed = false;

// Horizontally centered, flush with the top edge - no margin, so the pill
// reads as a notch attached to the screen edge rather than floating below it.
float3 CenteredOffset(float2 size)
{
    return { (static_cast<float>(kRenderWidth) - size.x) / 2.0f, 0.0f, 0.0f };
}

// --- Interaction-layer sync ------------------------------------------
// The compositor never reports animated values back to the app: reading an
// animating Visual property returns the value the app last set, and even
// AnimationController.Progress stays at 0 for the whole run (verified by
// polling it during the morph). So the interaction window can't sample the
// pill's live bounds - instead it replays the same morph the compositor is
// running (same endpoints, duration and easing curve), clocked from the
// compositor's own frame ticks so it advances exactly when the picture
// does, and snaps to the exact target at the end.
struct MorphTracker
{
    // Bounds the interaction window is currently snapped to (render-layer
    // client coordinates, same space as the pill's Offset/Size).
    float2 size = kCollapsedSize;
    float3 offset = CenteredOffset(kCollapsedSize);

    float2 fromSize{}, toSize{};
    float3 fromOffset{}, toOffset{};
    std::chrono::steady_clock::time_point start{};
    bool clockPending = false;         // start the clock on the next compositor tick
    std::atomic<bool> active{ false }; // the only field CompositorTickThread reads
} g_morph;
HANDLE g_morphStartedEvent{};

// The morph easing curve, evaluated on the app side. CSS-style cubic bezier
// from (0,0) to (1,1): find the curve parameter whose x equals t (bisection
// - x is monotonic while both control x's lie in [0, 1]), then take y there.
float MorphEase(float t)
{
    auto bezier = [](float p1, float p2, float u)
    {
        float v = 1.0f - u;
        return 3.0f * v * v * u * p1 + 3.0f * v * u * u * p2 + u * u * u;
    };
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 24; ++i)
    {
        float mid = (lo + hi) / 2.0f;
        (bezier(kMorphEaseControl1.x, kMorphEaseControl2.x, mid) < t ? lo : hi) = mid;
    }
    return bezier(kMorphEaseControl1.y, kMorphEaseControl2.y, (lo + hi) / 2.0f);
}

void SnapInteractionLayer(float2 size, float3 offset)
{
    g_morph.size = size;
    g_morph.offset = offset;
    SetWindowPos(g_inputHwnd, nullptr,
        g_renderLeft + static_cast<int>(std::lround(offset.x)),
        static_cast<int>(std::lround(offset.y)),
        static_cast<int>(std::lround(size.x)),
        static_cast<int>(std::lround(size.y)),
        SWP_NOZORDER | SWP_NOACTIVATE);
}

// Starts following a morph the implicit animations are about to run. The
// start point is wherever the interaction window is right now - which, if
// a previous morph is still in flight, is also (to within a frame) where
// the compositor picks the new animation up from.
void BeginMorphTracking(float2 toSize, float3 toOffset)
{
    g_morph.fromSize = g_morph.size;
    g_morph.fromOffset = g_morph.offset;
    g_morph.toSize = toSize;
    g_morph.toOffset = toOffset;
    g_morph.clockPending = true;
    g_morph.active = true;
    SetEvent(g_morphStartedEvent);
}

// Runs on the UI thread once per compositor frame while a morph is active.
void SyncInteractionLayer()
{
    if (!g_morph.active)
    {
        return;
    }
    auto now = std::chrono::steady_clock::now();
    if (g_morph.clockPending)
    {
        // First tick after the properties were set: the commit carrying
        // them lands on this frame, so this is when the compositor's own
        // clock for the animation starts too.
        g_morph.start = now;
        g_morph.clockPending = false;
    }
    float t = std::chrono::duration<float>(now - g_morph.start) / kMorphDuration;
    if (t >= 1.0f)
    {
        g_morph.active = false;
        SnapInteractionLayer(g_morph.toSize, g_morph.toOffset);
        return;
    }
    float e = MorphEase(t);
    SnapInteractionLayer(lerp(g_morph.fromSize, g_morph.toSize, e), lerp(g_morph.fromOffset, g_morph.toOffset, e));
}

// Tick source for the sync above: blocks on the compositor's frame clock
// while a morph is in flight (one wake per composed frame - the cadence the
// render layer is animating at, not an independent timer) and sleeps on an
// event otherwise, so an idle island costs nothing here.
void CompositorTickThread()
{
    for (;;)
    {
        WaitForSingleObject(g_morphStartedEvent, INFINITE);
        while (g_morph.active)
        {
            // Timeout only so the morph still finishes by wall clock if the
            // compositor clock ever stalls (e.g. display off).
            DCompositionWaitForCompositorClock(0, nullptr, 100);
            PostMessageW(g_inputHwnd, WM_APP_COMPOSITOR_TICK, 0, 0);
        }
    }
}

// Tactile press feedback: scale down on button-down, spring back (with a
// touch of overshoot) on release. Explicit KeyFrameAnimations, started
// directly on demand - independent of the Size/Offset/Opacity implicit
// animations set up in BuildScene, and of each other (each StartAnimation
// call simply replaces whatever was previously animating Scale).
void PlayPressDown()
{
    auto easing = g_compositor.CreateCubicBezierEasingFunction({ 0.4f, 0.0f }, { 0.2f, 1.0f });
    auto anim = g_compositor.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { 0.96f, 0.96f, 1.0f }, easing);
    anim.Duration(100ms);
    g_pill.StartAnimation(L"Scale", anim);
}

void PlayPressRelease()
{
    // Control points with Y > 1 overshoot past the target before settling -
    // a standard "back-out" easing shape - for a springy rather than flat
    // return to full size.
    auto easing = g_compositor.CreateCubicBezierEasingFunction({ 0.34f, 1.56f }, { 0.64f, 1.0f });
    auto anim = g_compositor.CreateVector3KeyFrameAnimation();
    anim.InsertKeyFrame(1.0f, { 1.0f, 1.0f, 1.0f }, easing);
    anim.Duration(220ms);
    g_pill.StartAnimation(L"Scale", anim);
}

DesktopWindowTarget CreateDesktopWindowTarget(Compositor const& compositor, HWND window)
{
    namespace abi = ABI::Windows::UI::Composition::Desktop;

    auto interop = compositor.as<abi::ICompositorDesktopInterop>();
    DesktopWindowTarget target{ nullptr };
    check_hresult(interop->CreateDesktopWindowTarget(
        window,
        /* isTopmost */ true,
        reinterpret_cast<abi::IDesktopWindowTarget**>(put_abi(target))));
    return target;
}

DispatcherQueueController CreateDispatcherQueueControllerForCurrentThread()
{
    DispatcherQueueOptions options{
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_STA
    };

    DispatcherQueueController controller{ nullptr };
    check_hresult(CreateDispatcherQueueController(
        options,
        reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(put_abi(controller))));
    return controller;
}

// Sets the target Size/Offset/Opacity for a state; the actual on-screen
// morph is produced entirely by the implicit animations attached in
// BuildScene - this function never touches a pixel itself. The interaction
// layer is told to follow the same morph.
void ApplyState(bool expanded, const char* reason)
{
    g_expanded = expanded;
    float2 size = expanded ? kExpandedSize : kCollapsedSize;
    float3 offset = CenteredOffset(size);

    g_pill.Size(size);
    g_pill.Offset(offset);
    g_content.Opacity(expanded ? 1.0f : 0.0f);
    BeginMorphTracking(size, offset);

    std::printf("[cup-island] %s (%s)\n", expanded ? "expanded" : "collapsed", reason);
    std::fflush(stdout);
}

void BuildScene(Compositor const& compositor, ContainerVisual const& root)
{
    auto easing = compositor.CreateCubicBezierEasingFunction(kMorphEaseControl1, kMorphEaseControl2);

    auto sizeAnim = compositor.CreateVector2KeyFrameAnimation();
    sizeAnim.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easing);
    sizeAnim.Duration(kMorphDuration);
    sizeAnim.Target(L"Size");

    auto offsetAnim = compositor.CreateVector3KeyFrameAnimation();
    offsetAnim.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easing);
    offsetAnim.Duration(kMorphDuration);
    offsetAnim.Target(L"Offset");

    auto pillImplicit = compositor.CreateImplicitAnimationCollection();
    pillImplicit.Insert(L"Size", sizeAnim);
    pillImplicit.Insert(L"Offset", offsetAnim);

    // Content fades in slightly after the pill starts growing, and (by
    // symmetry) fades out slightly after it starts shrinking - an echo of
    // the container-morphs-then-content-crossfades choreography real
    // Dynamic Island uses.
    auto opacityAnim = compositor.CreateScalarKeyFrameAnimation();
    opacityAnim.InsertExpressionKeyFrame(1.0f, L"this.FinalValue", easing);
    opacityAnim.Duration(200ms);
    opacityAnim.DelayTime(100ms);
    opacityAnim.DelayBehavior(AnimationDelayBehavior::SetInitialValueBeforeDelay);
    opacityAnim.Target(L"Opacity");

    auto contentImplicit = compositor.CreateImplicitAnimationCollection();
    contentImplicit.Insert(L"Opacity", opacityAnim);

    // --- Pill background -----------------------------------------------
    // A rounded rectangle whose corner radius always tracks half the
    // pill's own (animating) height via an ExpressionAnimation, so it never
    // needs per-frame app code to stay in shape while resizing.
    //
    // Only the bottom corners are rounded. CompositionRoundedRectangleGeometry
    // has a single radius for all four corners, so instead the geometry is
    // made one radius taller than the pill and shifted up by that radius:
    // its top corners land above the visual's top edge, where ShapeVisual's
    // clip to its own Size hides them, and what remains visible is square on
    // top and stadium-rounded at the bottom - a notch hanging from the
    // screen edge.
    g_pill = compositor.CreateShapeVisual();
    g_pill.Size(kCollapsedSize);
    g_pill.Offset(CenteredOffset(kCollapsedSize));
    g_pill.ImplicitAnimations(pillImplicit);

    // Keeps the press-feedback Scale transform pivoting around the middle
    // of the pill's top edge, at whatever size it is: the top stays glued
    // to the screen edge while the sides and bottom squash in, rather than
    // the whole pill detaching from the edge by a couple of pixels.
    auto centerPointExpr = compositor.CreateExpressionAnimation(L"Vector3(pill.Size.X / 2, 0, 0)");
    centerPointExpr.SetReferenceParameter(L"pill", g_pill);
    g_pill.StartAnimation(L"CenterPoint", centerPointExpr);

    auto bgGeometry = compositor.CreateRoundedRectangleGeometry();

    auto radiusExpr = compositor.CreateExpressionAnimation(
        L"Vector2(Min(pill.Size.X, pill.Size.Y) / 2, Min(pill.Size.X, pill.Size.Y) / 2)");
    radiusExpr.SetReferenceParameter(L"pill", g_pill);
    bgGeometry.StartAnimation(L"CornerRadius", radiusExpr);

    auto bgOffsetExpr = compositor.CreateExpressionAnimation(
        L"Vector2(0, -Min(pill.Size.X, pill.Size.Y) / 2)");
    bgOffsetExpr.SetReferenceParameter(L"pill", g_pill);
    bgGeometry.StartAnimation(L"Offset", bgOffsetExpr);

    auto bgSizeExpr = compositor.CreateExpressionAnimation(
        L"Vector2(pill.Size.X, pill.Size.Y + Min(pill.Size.X, pill.Size.Y) / 2)");
    bgSizeExpr.SetReferenceParameter(L"pill", g_pill);
    bgGeometry.StartAnimation(L"Size", bgSizeExpr);

    auto bgShape = compositor.CreateSpriteShape(bgGeometry);
    bgShape.FillBrush(compositor.CreateColorBrush({ 255, 0, 0, 0 }));
    g_pill.Shapes().Append(bgShape);

    // --- Content (icon + title/subtitle bars + progress track) ---------
    // A placeholder layout standing in for real media session data
    // (section 6, out of scope for this prototype). Every shape's position
    // is bound via expression to the pill's own Size, so it reflows
    // automatically as the pill morphs instead of needing manual re-layout.
    g_content = compositor.CreateContainerVisual();
    g_content.Opacity(0.0f);
    g_content.ImplicitAnimations(contentImplicit);

    auto artGeometry = compositor.CreateEllipseGeometry();
    artGeometry.Radius({ 16.0f, 16.0f });
    auto artCenterExpr = compositor.CreateExpressionAnimation(L"Vector2(30, pill.Size.Y / 2)");
    artCenterExpr.SetReferenceParameter(L"pill", g_pill);
    artGeometry.StartAnimation(L"Center", artCenterExpr);
    auto artShape = compositor.CreateSpriteShape(artGeometry);
    artShape.FillBrush(compositor.CreateColorBrush({ 255, 90, 200, 170 }));

    auto titleGeometry = compositor.CreateRoundedRectangleGeometry();
    titleGeometry.CornerRadius({ 5.0f, 5.0f });
    titleGeometry.Size({ 180.0f, 14.0f });
    auto titleOffsetExpr = compositor.CreateExpressionAnimation(L"Vector2(58, pill.Size.Y / 2 - 18)");
    titleOffsetExpr.SetReferenceParameter(L"pill", g_pill);
    titleGeometry.StartAnimation(L"Offset", titleOffsetExpr);
    auto titleShape = compositor.CreateSpriteShape(titleGeometry);
    titleShape.FillBrush(compositor.CreateColorBrush({ 230, 255, 255, 255 }));

    auto subtitleGeometry = compositor.CreateRoundedRectangleGeometry();
    subtitleGeometry.CornerRadius({ 4.0f, 4.0f });
    subtitleGeometry.Size({ 120.0f, 10.0f });
    auto subtitleOffsetExpr = compositor.CreateExpressionAnimation(L"Vector2(58, pill.Size.Y / 2 + 4)");
    subtitleOffsetExpr.SetReferenceParameter(L"pill", g_pill);
    subtitleGeometry.StartAnimation(L"Offset", subtitleOffsetExpr);
    auto subtitleShape = compositor.CreateSpriteShape(subtitleGeometry);
    subtitleShape.FillBrush(compositor.CreateColorBrush({ 140, 255, 255, 255 }));

    auto trackGeometry = compositor.CreateRoundedRectangleGeometry();
    trackGeometry.CornerRadius({ 2.0f, 2.0f });
    trackGeometry.Size({ 300.0f, 4.0f });
    auto trackOffsetExpr = compositor.CreateExpressionAnimation(L"Vector2(58, pill.Size.Y - 20)");
    trackOffsetExpr.SetReferenceParameter(L"pill", g_pill);
    trackGeometry.StartAnimation(L"Offset", trackOffsetExpr);
    auto trackShape = compositor.CreateSpriteShape(trackGeometry);
    trackShape.FillBrush(compositor.CreateColorBrush({ 60, 255, 255, 255 }));

    auto progressGeometry = compositor.CreateRoundedRectangleGeometry();
    progressGeometry.CornerRadius({ 2.0f, 2.0f });
    progressGeometry.Size({ 120.0f, 4.0f });
    auto progressOffsetExpr = compositor.CreateExpressionAnimation(L"Vector2(58, pill.Size.Y - 20)");
    progressOffsetExpr.SetReferenceParameter(L"pill", g_pill);
    progressGeometry.StartAnimation(L"Offset", progressOffsetExpr);
    auto progressShape = compositor.CreateSpriteShape(progressGeometry);
    progressShape.FillBrush(compositor.CreateColorBrush({ 210, 255, 255, 255 }));

    auto contentShapeVisual = compositor.CreateShapeVisual();
    auto contentSizeExpr = compositor.CreateExpressionAnimation(L"pill.Size");
    contentSizeExpr.SetReferenceParameter(L"pill", g_pill);
    contentShapeVisual.StartAnimation(L"Size", contentSizeExpr);
    contentShapeVisual.Shapes().Append(artShape);
    contentShapeVisual.Shapes().Append(titleShape);
    contentShapeVisual.Shapes().Append(subtitleShape);
    contentShapeVisual.Shapes().Append(trackShape);
    contentShapeVisual.Shapes().Append(progressShape);

    g_content.Children().InsertAtTop(contentShapeVisual);
    g_pill.Children().InsertAtTop(g_content);

    root.Children().InsertAtTop(g_pill);
}

// Window procedure of the interaction layer - the only window that ever
// receives input. (The render layer's class uses DefWindowProcW directly:
// WS_EX_TRANSPARENT means nothing reaches it that needs handling.)
LRESULT CALLBACK InputWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
try
{
    switch (message)
    {
    case WM_APP_COMPOSITOR_TICK:
        SyncInteractionLayer();
        return 0;

    case WM_LBUTTONDOWN:
        // This window is exactly the pill, so any press here is a press on
        // the pill.
        g_pressed = true;
        SetCapture(hwnd);
        PlayPressDown();
        return 0;

    case WM_LBUTTONUP:
        if (g_pressed)
        {
            g_pressed = false;
            ReleaseCapture();
            PlayPressRelease();

            // Capture means we get this even if the press was dragged off
            // the pill before releasing - only toggle if it's still on the
            // pill, same "no margin" rule as an ordinary click.
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT client{};
            GetClientRect(hwnd, &client);
            if (PtInRect(&client, pt))
            {
                ApplyState(!g_expanded, "click");
            }
        }
        return 0;

    case WM_CAPTURECHANGED:
        // Capture was taken away from us by something other than our own
        // ReleaseCapture() call above (which already clears g_pressed
        // first) - e.g. another app forcing capture. Spring back rather
        // than leaving the pill stuck scaled-down.
        if (g_pressed)
        {
            g_pressed = false;
            PlayPressRelease();
        }
        return 0;

    case WM_ACTIVATE:
        // Losing activation means some other window (or the desktop) was
        // just clicked - collapse, mirroring Dynamic Island's dismiss-on-
        // tap-away behavior. A click outside the pill never reaches this
        // app at all (the render layer is excluded from hit-testing and
        // this window is only ever as big as the pill), so it activates
        // whatever it landed on instead, deactivating us.
        if (LOWORD(wParam) == WA_INACTIVE && g_expanded)
        {
            ApplyState(false, "deactivate");
        }
        break;

    case WM_TIMER:
        // One-shot only: kicks off an unattended demo right after launch so
        // the morph is visible without requiring a click. Nothing re-arms
        // these timers, so once both have fired the app is purely
        // event-driven (mouse clicks) again, per section 5's idle/no-timer
        // requirement.
        if (wParam == 1) { KillTimer(hwnd, 1); ApplyState(true, "demo"); SetTimer(hwnd, 2, 1600, nullptr); }
        else if (wParam == 2) { KillTimer(hwnd, 2); if (g_expanded) { ApplyState(false, "demo"); } }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        DestroyWindow(g_renderHwnd);
        g_renderHwnd = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
catch (winrt::hresult_error const& e)
{
    // Guards against a WinRT call failing mid-message-handling (e.g. a
    // transient Composition/graphics device issue) silently killing the
    // whole process - a C++ exception escaping a window procedure called
    // by the OS's own dispatch loop generally terminates the process
    // without ever printing anything, which is worse than a visibly
    // skipped frame.
    std::printf("[cup-island] WinRT error handling message 0x%04x: 0x%08x %ls\n",
        message, static_cast<unsigned int>(e.code()), e.message().c_str());
    std::fflush(stdout);
    return 0;
}
catch (...)
{
    std::printf("[cup-island] Unknown exception handling message 0x%04x\n", message);
    std::fflush(stdout);
    return 0;
}

} // namespace

int main()
{
    std::printf("Cup Island - minimal animation showoff prototype\n");
    std::printf("Click the pill to expand/collapse. Esc to quit.\n\n");
    std::fflush(stdout);

    init_apartment(apartment_type::single_threaded);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Windows.UI.Composition needs a DispatcherQueue on whatever thread
    // creates the Compositor; this adapts our plain Win32 message-pump
    // thread into one.
    auto queueController = CreateDispatcherQueueControllerForCurrentThread();

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW renderClass{ sizeof(renderClass) };
    renderClass.lpfnWndProc = DefWindowProcW;
    renderClass.hInstance = instance;
    renderClass.lpszClassName = kRenderClassName;
    RegisterClassExW(&renderClass);

    WNDCLASSEXW inputClass{ sizeof(inputClass) };
    inputClass.lpfnWndProc = InputWndProc;
    inputClass.hInstance = instance;
    inputClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    inputClass.lpszClassName = kInputClassName;
    RegisterClassExW(&inputClass);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    g_renderLeft = (screenWidth - kRenderWidth) / 2;

    // Render layer. WS_EX_NOREDIRECTIONBITMAP hands all pixel ownership to
    // DirectComposition - no GDI redirection surface, so areas the pill
    // doesn't paint are simply transparent with no extra work.
    // WS_EX_LAYERED | WS_EX_TRANSPARENT removes the window from hit-testing
    // entirely, which is the only way to pass mouse input through to other
    // processes' windows: returning HTTRANSPARENT from WM_NCHITTEST only
    // forwards to windows on the same thread and swallows the click
    // otherwise (the "can't click next to the pill" bug this replaces).
    // WS_EX_NOACTIVATE so it can never take focus either.
    g_renderHwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kRenderClassName, L"Cup Island Render",
        WS_POPUP,
        g_renderLeft, 0, kRenderWidth, kRenderHeight,
        nullptr, nullptr, instance, nullptr);

    if (!g_renderHwnd)
    {
        std::printf("Failed to create render window (0x%08lx)\n", GetLastError());
        return 1;
    }

    // Interaction layer. No redirection bitmap and no composition target, so
    // it draws nothing at all - yet, being an ordinary (non-layered) window,
    // it is hit-tested by its rectangle, which is kept equal to the pill's.
    float3 collapsedOffset = CenteredOffset(kCollapsedSize);
    g_inputHwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kInputClassName, L"Cup Island Prototype",
        WS_POPUP,
        g_renderLeft + static_cast<int>(collapsedOffset.x), static_cast<int>(collapsedOffset.y),
        static_cast<int>(kCollapsedSize.x), static_cast<int>(kCollapsedSize.y),
        nullptr, nullptr, instance, nullptr);

    if (!g_inputHwnd)
    {
        std::printf("Failed to create interaction window (0x%08lx)\n", GetLastError());
        return 1;
    }

    Compositor compositor;
    g_compositor = compositor;
    auto target = CreateDesktopWindowTarget(compositor, g_renderHwnd);
    auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    target.Root(root);

    BuildScene(compositor, root);

    g_morphStartedEvent = CreateEventW(nullptr, /* manualReset */ FALSE, /* initialState */ FALSE, nullptr);
    std::thread(CompositorTickThread).detach();

    ShowWindow(g_renderHwnd, SW_SHOWNOACTIVATE);
    ShowWindow(g_inputHwnd, SW_SHOW);
    SetTimer(g_inputHwnd, 1, 700, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    queueController.ShutdownQueueAsync().get();
    return static_cast<int>(msg.wParam);
}
