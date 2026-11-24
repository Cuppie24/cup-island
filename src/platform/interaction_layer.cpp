#include "platform/interaction_layer.h"
#include "platform/log.h"
#include "platform/single_instance.h"
#include "platform/watchdog.h"

#include "core/tokens.h"

#include <dwmapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <exception>
#include <thread>

namespace cup::platform {
namespace {

constexpr const wchar_t* kClassName = kInteractionWindowClass;

// Posted by the tick thread once per composed frame while a morph is running.
constexpr UINT WM_APP_COMPOSITOR_TICK = WM_APP + 1;
// Posted by setTimerFromAnyThread: wParam = TimerId, lParam = delay in ms.
constexpr UINT WM_APP_SET_TIMER = WM_APP + 2;

// GUID_CONSOLE_DISPLAY_STATE, spelled out: winnt.h only declares it.
constexpr GUID kConsoleDisplayState{ 0x6fe69556, 0x704a, 0x47a0, { 0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47 } };

HWND g_hwnd{}; // the first one made, the island's: the app's timers, hotkeys and posted messages are its
DWORD g_screenState = 1; // on; logged when it changes
bool g_textCursor = false;    // over a text field: the I-beam (decision 043)

void showCursor() { SetCursor(LoadCursorW(nullptr, g_textCursor ? IDC_IBEAM : IDC_ARROW)); }

// The compositor never reports animated values back to the app, so this
// replays the same morph (endpoints, duration, easing) clocked from the
// compositor's own frame ticks. See decision 001.
struct MorphFollower {
    Vec2 size, offset; // bounds the window is snapped to now (render-layer coordinates)
    Vec2 fromSize, toSize, fromOffset, toOffset;
    Motion motion;
    Vec2 fling; // pixels per ms at the start: the offset's throw
    std::chrono::steady_clock::time_point start{};
    bool clockPending = false;         // start the clock on the next compositor tick
    std::atomic<bool> active{ false }; // the only field the tick thread reads
};
HANDLE g_morphStartedEvent{};

// One interaction window: the island's, and the file dialog's island's
// (decision 181). Each follows its own island's morph.
struct Layer {
    HWND hwnd{};
    int renderLeft = 0;
    int renderTop = 0; // monitors above or below the primary don't start at y = 0
    InteractionCallbacks callbacks;
    bool pressed = false;
    bool trackingLeave = false; // TrackMouseEvent armed: WM_MOUSELEAVE will come
    MorphFollower morph;
};
// Never moved (the tick thread reads their `active`), and the count only grows.
constexpr int kMaxLayers = 2;
Layer g_layers[kMaxLayers];
std::atomic<int> g_layerCount{ 0 };
Layer* g_creating = nullptr; // messages sent while its window is being created are its

Layer* layerOf(HWND hwnd)
{
    for (int i = 0; i < g_layerCount; ++i)
        if (g_layers[i].hwnd == hwnd) return &g_layers[i];
    return g_creating;
}

bool anyMorphActive()
{
    for (int i = 0; i < g_layerCount; ++i)
        if (g_layers[i].morph.active) return true;
    return false;
}

void snap(Layer& layer, Vec2 size, Vec2 offset)
{
    layer.morph.size = size;
    layer.morph.offset = offset;
    SetWindowPos(layer.hwnd, nullptr,
        layer.renderLeft + static_cast<int>(std::lround(offset.x)),
        layer.renderTop + static_cast<int>(std::lround(offset.y)),
        static_cast<int>(std::lround(size.x)),
        static_cast<int>(std::lround(size.y)),
        SWP_NOZORDER | SWP_NOACTIVATE);
}

// UI thread, once per compositor frame while a morph is active.
void tickMorph(Layer& layer)
{
    MorphFollower& morph = layer.morph;
    if (!morph.active) return;

    auto now = std::chrono::steady_clock::now();
    if (morph.clockPending) {
        // The commit carrying the new targets lands on this frame, so the
        // compositor's clock for the animation starts now too.
        morph.start = now;
        morph.clockPending = false;
    }
    const Motion& m = morph.motion;
    float t = std::chrono::duration<float>(now - morph.start) / std::max(m.duration, Ms{ 1 });
    if (t >= 1.0f) {
        morph.active = false;
        snap(layer, morph.toSize, morph.toOffset);
        return;
    }
    // The renderer's keyframes are samples of this same curve (platform/spring_motion.h).
    float e = springProgress(m, t), push = springCarry(m, t) * static_cast<float>(m.duration.count());
    Vec2 offset = lerp(morph.fromOffset, morph.toOffset, e);
    snap(layer, lerp(morph.fromSize, morph.toSize, e), { offset.x + morph.fling.x * push, offset.y + morph.fling.y * push });
}

void onCompositorTick()
{
    for (int i = 0; i < g_layerCount; ++i) tickMorph(g_layers[i]);
}

// Blocks until the next composed frame. Windows 11 has a compositor clock
// API; it's looked up at runtime, never linked: a static import made the exe
// refuse to start on Windows 10 ("ordinal 1104 not found", decision 001).
// Windows 10 waits with DwmFlush, which also returns once per composition.
using WaitForCompositorClock = DWORD(WINAPI*)(UINT count, const HANDLE* handles, DWORD timeoutMs);

WaitForCompositorClock findCompositorClock()
{
    HMODULE dcomp = LoadLibraryW(L"dcomp.dll");
    auto wait = dcomp ? reinterpret_cast<WaitForCompositorClock>(
                            reinterpret_cast<void*>(GetProcAddress(dcomp, "DCompositionWaitForCompositorClock")))
                      : nullptr;
    logLine("frame clock: %s", wait ? "compositor clock (Windows 11)" : "DwmFlush (Windows 10 fallback)");
    return wait;
}

void waitForNextFrame(WaitForCompositorClock compositorClock)
{
    if (compositorClock) {
        // Timeout only so a morph still finishes if the clock stalls (e.g. display off).
        compositorClock(0, nullptr, 100);
        return;
    }
    // DwmFlush can return at once when nothing is being composed; never spin.
    auto start = std::chrono::steady_clock::now();
    DwmFlush();
    if (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1)) Sleep(4);
}

// Wakes once per composed frame while a morph runs (the render layer's own
// cadence, not an independent timer) and sleeps otherwise: idle costs nothing.
void compositorTickThread()
{
    WaitForCompositorClock compositorClock = findCompositorClock();
    for (;;) {
        WaitForSingleObject(g_morphStartedEvent, INFINITE);
        while (anyMorphActive()) {
            waitForNextFrame(compositorClock);
            PostMessageW(g_hwnd, WM_APP_COMPOSITOR_TICK, 0, 0);
        }
    }
}

// Client coordinates -> render-layer coordinates. Signed: with capture, a
// drag reports points left of / above the window as negative.
Vec2 pointFrom(const Layer& layer, LPARAM lParam)
{
    return { static_cast<float>(GET_X_LPARAM(lParam)) + layer.morph.offset.x, static_cast<float>(GET_Y_LPARAM(lParam)) + layer.morph.offset.y };
}

bool inside(HWND hwnd, LPARAM lParam)
{
    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    RECT client{};
    GetClientRect(hwnd, &client);
    return PtInRect(&client, pt) != FALSE;
}

template <typename F, typename... Args>
void call(const F& callback, Args... args)
{
    if (callback) callback(args...);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
try {
    Layer* at = layerOf(hwnd);
    if (!at) return DefWindowProcW(hwnd, message, wParam, lParam);
    Layer& layer = *at;
    // A registered message isn't a compile-time constant, so it can't be a case.
    if (message == showYourselfMessage()) {
        call(layer.callbacks.onShowRequested);
        return 0;
    }
    if (message == watchdogPing()) { // posted: handled only once this thread is back in its loop (decision 144)
        answerWatchdog();
        return 0;
    }
    switch (message) {
    case WM_APP_COMPOSITOR_TICK:
        onCompositorTick();
        return 0;

    case WM_APP_SET_TIMER:
        setTimer(static_cast<TimerId>(wParam), std::chrono::milliseconds{ lParam });
        return 0;

    case WM_LBUTTONDOWN: // this window is exactly the pill (and bubble)
        layer.pressed = true;
        SetCapture(hwnd);
        call(layer.callbacks.onPressDown, pointFrom(layer, lParam));
        return 0;

    case WM_LBUTTONUP:
        if (layer.pressed) {
            // Cleared before ReleaseCapture, so the WM_CAPTURECHANGED it sends isn't a cancel.
            layer.pressed = false;
            ReleaseCapture();
            // With capture we get this even if the press was dragged off the window.
            call(layer.callbacks.onPressRelease, pointFrom(layer, lParam), inside(hwnd, lParam) ? Release::Inside : Release::Outside);
        }
        return 0;

    case WM_RBUTTONUP: // a row's menu, at the pointer (decision 086); not on the way down, as Windows' own menus
        if (!layer.pressed) call(layer.callbacks.onRightClick, pointFrom(layer, lParam));
        return 0;

    case WM_CAPTURECHANGED:
        // Capture taken by someone else mid-press: spring back, don't stick.
        if (layer.pressed) {
            layer.pressed = false;
            call(layer.callbacks.onPressRelease, Vec2{}, Release::Cancelled);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (!layer.trackingLeave) {
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hwnd, 0 };
            layer.trackingLeave = TrackMouseEvent(&track) != FALSE;
        }
        call(layer.callbacks.onPointerMoved, pointFrom(layer, lParam), inside(hwnd, lParam));
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) != HTCLIENT) break;
        showCursor();
        return TRUE;

    case WM_MOUSEWHEEL: {
        // Notches, positive = towards the user (down). Touchpads send fractions
        // of a notch as they go: passed on as they come, so lists glide with
        // the fingers (decision 029).
        float notches = -static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; // screen coordinates, unlike the other mouse messages
        ScreenToClient(hwnd, &pt);
        if (notches != 0) call(layer.callbacks.onWheel, pointFrom(layer, MAKELPARAM(pt.x, pt.y)), notches);
        return 0;
    }

    case WM_MOUSELEAVE: // hover ends; one-shot, re-armed by the next move
        layer.trackingLeave = false;
        call(layer.callbacks.onPointerLeft);
        return 0;

    case WM_ACTIVATE:
        // A click outside the pill never reaches us (render layer is
        // click-through), so it activates whatever it hit, deactivating us.
        if (LOWORD(wParam) == WA_INACTIVE) call(layer.callbacks.onClickOutside);
        break;

    case WM_TIMER:
        KillTimer(hwnd, wParam); // all timers are one-shot
        call(layer.callbacks.onTimer, static_cast<TimerId>(wParam));
        return 0;

    case WM_KEYDOWN:
        call(layer.callbacks.onKeyDown, wParam);
        return 0;

    case WM_SYSKEYDOWN: // with Alt held: a shortcut being recorded needs it (decision 032)
        call(layer.callbacks.onKeyDown, wParam);
        break;           // and Windows' own handling still runs (Alt+Space, Alt+Tab...)

    case WM_SYSCOMMAND:
        // Alt+F4 arrives as SC_CLOSE: the island isn't a window to close by
        // accident (decision 049). Quitting is the tray's Exit; the updater,
        // the installer and shutdown don't come this way.
        if ((wParam & 0xFFF0) == SC_CLOSE) return 0;
        break;

    case WM_HOTKEY:
        call(layer.callbacks.onHotkey, wParam);
        return 0;

    case WM_CHAR:
        call(layer.callbacks.onChar, static_cast<wchar_t>(wParam));
        return 0;

    case WM_DPICHANGED:
        // Handled, not passed on: the app resizes everything itself (tokens are
        // rescaled), so Windows' suggested rectangle for this window is ignored.
        call(layer.callbacks.onDpiChanged, static_cast<float>(LOWORD(wParam)) / 96.0f);
        return 0;

    case WM_DISPLAYCHANGE: // sent to every top-level window
        call(layer.callbacks.onDisplaysChanged);
        return 0;

    case WM_SETTINGCHANGE: // also to every top-level window; wParam names what changed
        if (wParam == SPI_SETCLIENTAREAANIMATION) call(layer.callbacks.onAnimationSettingChanged);
        // "Transparency effects" says only this: the colour set changed.
        if (lParam && std::wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0) call(layer.callbacks.onColourSettingChanged);
        break;

    case WM_POWERBROADCAST: // only logged: a user's log then shows what happened around a sleep (decisions 060, 144)
        if (wParam == PBT_APMSUSPEND) logLine("going to sleep");
        else if (wParam == PBT_APMRESUMEAUTOMATIC) logLine("woke from sleep");
        else if (wParam == PBT_POWERSETTINGCHANGE && lParam) {
            const auto* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
            if (setting->PowerSetting == kConsoleDisplayState && setting->DataLength >= sizeof(DWORD)) {
                DWORD state = *reinterpret_cast<const DWORD*>(setting->Data); // 0 off, 1 on, 2 dimmed
                if (state != g_screenState) logLine("screen %s", state == 0 ? "off" : state == 1 ? "on" : "dimmed");
                g_screenState = state;
            }
        }
        return TRUE;

    case WM_DESTROY:
        if (hwnd == g_hwnd) PostQuitMessage(0); // the island's: the app ends with it
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
catch (winrt::hresult_error const& e) {
    // An exception escaping a window procedure usually kills the process
    // silently; a visibly skipped frame is better.
    logLine("WinRT error handling message 0x%04x: 0x%08x %ls",
        message, static_cast<unsigned int>(e.code()), e.message().c_str());
    return 0;
}
catch (std::exception const& e) {
    logLine("Exception handling message 0x%04x: %s", message, e.what());
    return 0;
}
catch (...) {
    logLine("Unknown exception handling message 0x%04x", message);
    return 0;
}

} // namespace

HWND createInteractionLayer(HINSTANCE instance, int renderLeft, int renderTop, Vec2 size, Vec2 offset, InteractionCallbacks callbacks)
{
    int index = g_layerCount;
    if (index >= kMaxLayers) return nullptr;
    bool first = index == 0;
    Layer& layer = g_layers[index];
    layer.renderLeft = renderLeft;
    layer.renderTop = renderTop;
    layer.callbacks = std::move(callbacks);
    layer.morph.size = size;
    layer.morph.offset = offset;

    // The island's class is how a second launch finds the running island
    // (single_instance.h): another island's window mustn't answer to it.
    const wchar_t* className = first ? kClassName : L"CupIslandInputWindow2";
    WNDCLASSEXW windowClass{ sizeof(windowClass) };
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    RegisterClassExW(&windowClass);

    // No redirection bitmap and no composition target: draws nothing, but as
    // an ordinary (non-layered) window it's hit-tested by its rectangle.
    g_creating = &layer;
    layer.hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        className, L"Cup Island",
        WS_POPUP,
        renderLeft + static_cast<int>(offset.x), renderTop + static_cast<int>(offset.y),
        static_cast<int>(size.x), static_cast<int>(size.y),
        nullptr, nullptr, instance, nullptr);
    g_creating = nullptr;
    if (!layer.hwnd) return nullptr;
    g_layerCount = index + 1;
    if (!first) return layer.hwnd;
    g_hwnd = layer.hwnd;

    // Sleep and wake in the log (decisions 060, 144). A Modern Standby PC
    // doesn't broadcast them: a user's five days of logs had not one line.
    // Registered, the window gets them wherever Windows sends them, and the
    // screen going off and on marks a standby on either kind of PC.
    RegisterSuspendResumeNotification(g_hwnd, DEVICE_NOTIFY_WINDOW_HANDLE);                // Windows 8+
    RegisterPowerSettingNotification(g_hwnd, &kConsoleDisplayState, DEVICE_NOTIFY_WINDOW_HANDLE); // Windows 8+

    g_morphStartedEvent = CreateEventW(nullptr, /* manualReset */ FALSE, /* initialState */ FALSE, nullptr);
    std::thread(compositorTickThread).detach();
    return g_hwnd;
}

void setRenderOrigin(HWND window, int renderLeft, int renderTop, std::optional<Vec2> offset)
{
    Layer* layer = layerOf(window);
    if (!layer) return;
    layer->renderLeft = renderLeft;
    layer->renderTop = renderTop;
    if (offset) layer->morph.active = false; // it's put there, not on its way
    snap(*layer, layer->morph.size, offset.value_or(layer->morph.offset));
}

void followMorph(HWND window, Vec2 toSize, Vec2 toOffset, const SceneMotion& motion)
{
    Layer* layer = layerOf(window);
    if (!layer) return;
    MorphFollower& morph = layer->morph;
    // Start from wherever the window is now: if a morph is mid-flight, that's
    // also (within a frame) where the compositor picks the new one up from.
    morph.fromSize = morph.size;
    morph.fromOffset = morph.offset;
    morph.toSize = toSize;
    morph.toOffset = toOffset;
    morph.motion = towardSize(motion.morph, toSize);
    morph.fling = motion.fling;
    morph.clockPending = true;
    morph.active = true;
    SetEvent(g_morphStartedEvent);
}

void setTextCursor(bool text)
{
    if (text == g_textCursor) return;
    g_textCursor = text;
    // Now, not at the next move (WM_SETCURSOR came before the move that changed
    // it); but only over us, or another window's cursor would flicker.
    POINT at{};
    if (!GetCursorPos(&at)) return;
    HWND under = WindowFromPoint(at);
    for (int i = 0; i < g_layerCount; ++i)
        if (g_layers[i].hwnd == under) showCursor();
}

void jumpTo(HWND window, Vec2 size, Vec2 offset)
{
    Layer* layer = layerOf(window);
    if (!layer) return;
    layer->morph.active = false;
    snap(*layer, size, offset);
}

void setTimer(TimerId id, std::optional<std::chrono::milliseconds> delay)
{
    auto timerId = static_cast<UINT_PTR>(id);
    if (!delay) {
        KillTimer(g_hwnd, timerId);
        return;
    }
    auto ms = std::max<long long>(delay->count(), USER_TIMER_MINIMUM);
    SetTimer(g_hwnd, timerId, static_cast<UINT>(ms), nullptr);
}

void setTimerFromAnyThread(TimerId id, std::chrono::milliseconds delay)
{
    PostMessageW(g_hwnd, WM_APP_SET_TIMER, static_cast<WPARAM>(id), static_cast<LPARAM>(delay.count()));
}

} // namespace cup::platform
