#include "platform/interaction_layer.h"

#include "platform/drop_target.h"
#include "platform/log.h"
#include "platform/single_instance.h"

#include "core/easing.h"
#include "core/tokens.h"

#include <dwmapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

namespace cup::platform {
namespace {

constexpr const wchar_t* kClassName = kInteractionWindowClass;

// Posted by the tick thread once per composed frame while a morph is running.
constexpr UINT WM_APP_COMPOSITOR_TICK = WM_APP + 1;
// Posted by setTimerFromAnyThread: wParam = TimerId, lParam = delay in ms.
constexpr UINT WM_APP_SET_TIMER = WM_APP + 2;

HWND g_hwnd{};
int g_renderLeft = 0;
int g_renderTop = 0; // monitors above or below the primary don't start at y = 0
InteractionCallbacks g_callbacks;
bool g_pressed = false;
bool g_trackingLeave = false; // TrackMouseEvent armed: WM_MOUSELEAVE will come
bool g_textCursor = false;    // over a text field: the I-beam (decision 043)

void showCursor() { SetCursor(LoadCursorW(nullptr, g_textCursor ? IDC_IBEAM : IDC_ARROW)); }

// The compositor never reports animated values back to the app, so this
// replays the same morph (endpoints, duration, easing) clocked from the
// compositor's own frame ticks. See decision 001.
struct MorphFollower {
    Vec2 size, offset; // bounds the window is snapped to now (render-layer coordinates)
    Vec2 fromSize, toSize, fromOffset, toOffset;
    std::chrono::steady_clock::time_point start{};
    bool clockPending = false;         // start the clock on the next compositor tick
    std::atomic<bool> active{ false }; // the only field the tick thread reads
} g_morph;
HANDLE g_morphStartedEvent{};

void snap(Vec2 size, Vec2 offset)
{
    g_morph.size = size;
    g_morph.offset = offset;
    SetWindowPos(g_hwnd, nullptr,
        g_renderLeft + static_cast<int>(std::lround(offset.x)),
        g_renderTop + static_cast<int>(std::lround(offset.y)),
        static_cast<int>(std::lround(size.x)),
        static_cast<int>(std::lround(size.y)),
        SWP_NOZORDER | SWP_NOACTIVATE);
}

// UI thread, once per compositor frame while a morph is active.
void onCompositorTick()
{
    if (!g_morph.active) return;

    auto now = std::chrono::steady_clock::now();
    if (g_morph.clockPending) {
        // The commit carrying the new targets lands on this frame, so the
        // compositor's clock for the animation starts now too.
        g_morph.start = now;
        g_morph.clockPending = false;
    }
    float t = std::chrono::duration<float>(now - g_morph.start) / tokens().morphDuration;
    if (t >= 1.0f) {
        g_morph.active = false;
        snap(g_morph.toSize, g_morph.toOffset);
        return;
    }
    float e = cubicBezierEase(tokens().morphEase, t);
    snap(lerp(g_morph.fromSize, g_morph.toSize, e), lerp(g_morph.fromOffset, g_morph.toOffset, e));
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
        while (g_morph.active) {
            waitForNextFrame(compositorClock);
            PostMessageW(g_hwnd, WM_APP_COMPOSITOR_TICK, 0, 0);
        }
    }
}

// Client coordinates -> render-layer coordinates. Signed: with capture, a
// drag reports points left of / above the window as negative.
Vec2 pointFrom(LPARAM lParam)
{
    return { static_cast<float>(GET_X_LPARAM(lParam)) + g_morph.offset.x, static_cast<float>(GET_Y_LPARAM(lParam)) + g_morph.offset.y };
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
    // A registered message isn't a compile-time constant, so it can't be a case.
    if (message == showYourselfMessage()) {
        call(g_callbacks.onShowRequested);
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
        g_pressed = true;
        SetCapture(hwnd);
        call(g_callbacks.onPressDown, pointFrom(lParam));
        return 0;

    case WM_LBUTTONUP:
        if (g_pressed) {
            // Cleared before ReleaseCapture, so the WM_CAPTURECHANGED it sends isn't a cancel.
            g_pressed = false;
            ReleaseCapture();
            // With capture we get this even if the press was dragged off the window.
            call(g_callbacks.onPressRelease, pointFrom(lParam), inside(hwnd, lParam) ? Release::Inside : Release::Outside);
        }
        return 0;

    case WM_CAPTURECHANGED:
        // Capture taken by someone else mid-press: spring back, don't stick.
        if (g_pressed) {
            g_pressed = false;
            call(g_callbacks.onPressRelease, Vec2{}, Release::Cancelled);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (!g_trackingLeave) {
            TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, hwnd, 0 };
            g_trackingLeave = TrackMouseEvent(&track) != FALSE;
        }
        call(g_callbacks.onPointerMoved, pointFrom(lParam), inside(hwnd, lParam));
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
        if (notches != 0) call(g_callbacks.onWheel, pointFrom(MAKELPARAM(pt.x, pt.y)), notches);
        return 0;
    }

    case WM_MOUSELEAVE: // hover ends; one-shot, re-armed by the next move
        g_trackingLeave = false;
        call(g_callbacks.onPointerLeft);
        return 0;

    case WM_ACTIVATE:
        // A click outside the pill never reaches us (render layer is
        // click-through), so it activates whatever it hit, deactivating us.
        if (LOWORD(wParam) == WA_INACTIVE) call(g_callbacks.onClickOutside);
        break;

    case WM_TIMER:
        KillTimer(hwnd, wParam); // all timers are one-shot
        call(g_callbacks.onTimer, static_cast<TimerId>(wParam));
        return 0;

    case WM_KEYDOWN:
        call(g_callbacks.onKeyDown, wParam);
        return 0;

    case WM_SYSKEYDOWN: // with Alt held: a shortcut being recorded needs it (decision 032)
        call(g_callbacks.onKeyDown, wParam);
        break;           // and Windows' own handling still runs (Alt+Space, Alt+Tab...)

    case WM_SYSCOMMAND:
        // Alt+F4 arrives as SC_CLOSE: the island isn't a window to close by
        // accident (decision 049). Quitting is the tray's Exit; the updater,
        // the installer and shutdown don't come this way.
        if ((wParam & 0xFFF0) == SC_CLOSE) return 0;
        break;

    case WM_HOTKEY:
        call(g_callbacks.onHotkey, wParam);
        return 0;

    case WM_CHAR:
        call(g_callbacks.onChar, static_cast<wchar_t>(wParam));
        return 0;

    case WM_DPICHANGED:
        // Handled, not passed on: the app resizes everything itself (tokens are
        // rescaled), so Windows' suggested rectangle for this window is ignored.
        call(g_callbacks.onDpiChanged, static_cast<float>(LOWORD(wParam)) / 96.0f);
        return 0;

    case WM_DISPLAYCHANGE: // sent to every top-level window
        call(g_callbacks.onDisplaysChanged);
        return 0;

    case WM_POWERBROADCAST: // only logged: a user's log then shows what happened around a sleep (decision 060)
        if (wParam == PBT_APMSUSPEND) logLine("going to sleep");
        else if (wParam == PBT_APMRESUMEAUTOMATIC) logLine("woke from sleep");
        return TRUE;

    case WM_DESTROY:
        revokeDropTarget(hwnd); // OLE wants it gone before the window (decision 061)
        PostQuitMessage(0);
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
catch (...) {
    logLine("Unknown exception handling message 0x%04x", message);
    return 0;
}

} // namespace

HWND createInteractionLayer(HINSTANCE instance, int renderLeft, int renderTop, Vec2 size, Vec2 offset, InteractionCallbacks callbacks)
{
    g_renderLeft = renderLeft;
    g_renderTop = renderTop;
    g_callbacks = std::move(callbacks);
    g_morph.size = size;
    g_morph.offset = offset;

    WNDCLASSEXW windowClass{ sizeof(windowClass) };
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kClassName;
    RegisterClassExW(&windowClass);

    // No redirection bitmap and no composition target: draws nothing, but as
    // an ordinary (non-layered) window it's hit-tested by its rectangle.
    g_hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kClassName, L"Cup Island",
        WS_POPUP,
        renderLeft + static_cast<int>(offset.x), renderTop + static_cast<int>(offset.y),
        static_cast<int>(size.x), static_cast<int>(size.y),
        nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) return nullptr;

    g_morphStartedEvent = CreateEventW(nullptr, /* manualReset */ FALSE, /* initialState */ FALSE, nullptr);
    std::thread(compositorTickThread).detach();
    return g_hwnd;
}

void setRenderOrigin(int renderLeft, int renderTop)
{
    g_renderLeft = renderLeft;
    g_renderTop = renderTop;
    snap(g_morph.size, g_morph.offset);
}

void followMorph(Vec2 toSize, Vec2 toOffset)
{
    // Start from wherever the window is now: if a morph is mid-flight, that's
    // also (within a frame) where the compositor picks the new one up from.
    g_morph.fromSize = g_morph.size;
    g_morph.fromOffset = g_morph.offset;
    g_morph.toSize = toSize;
    g_morph.toOffset = toOffset;
    g_morph.clockPending = true;
    g_morph.active = true;
    SetEvent(g_morphStartedEvent);
}

void setTextCursor(bool text)
{
    if (text == g_textCursor) return;
    g_textCursor = text;
    // Now, not at the next move (WM_SETCURSOR came before the move that changed
    // it); but only over us, or another window's cursor would flicker.
    POINT at{};
    if (GetCursorPos(&at) && WindowFromPoint(at) == g_hwnd) showCursor();
}

void jumpTo(Vec2 size, Vec2 offset)
{
    g_morph.active = false;
    snap(size, offset);
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
