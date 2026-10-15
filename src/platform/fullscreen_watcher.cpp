#include "platform/fullscreen_watcher.h"
#include "platform/log.h"
#include "platform/window_name.h"

#include "core/fullscreen.h"
#include "core/tokens.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <cstdio>
#include <cwchar>

namespace cup::platform {
namespace {

HWND g_island{};
std::function<void(bool)> g_onChange;
bool g_hidden = false;
HWINEVENTHOOK g_locationHook{};
DWORD g_locationProcess = 0;
UINT_PTR g_recheck = 0; // while hidden: Windows' exclusive flag can clear with no window event

Rect toRect(const RECT& r)
{
    return { static_cast<float>(r.left), static_cast<float>(r.top), static_cast<float>(r.right - r.left),
             static_cast<float>(r.bottom - r.top) };
}

// Windows that cover the monitor without being an app in fullscreen.
bool isShellSurface(HWND hwnd)
{
    static const wchar_t* const kClasses[] = {
        L"Progman", L"WorkerW",                                  // desktop
        L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",             // taskbars
        L"XamlExplorerHostIslandWindow", L"MultitaskingViewFrame", // Alt-Tab, Task View (Win11 / Win10)
        L"ForegroundStaging", L"Windows.UI.Core.CoreWindow",     // Start, search, notification center
    };
    wchar_t name[128]{};
    GetClassNameW(hwnd, name, 128);
    for (const wchar_t* cls : kClasses)
        if (std::wcscmp(name, cls) == 0) return true;
    return false;
}

std::optional<ForegroundWindow> describe(HWND hwnd)
{
    if (!hwnd) return std::nullopt;
    ForegroundWindow w;

    DWORD process = 0;
    GetWindowThreadProcessId(hwnd, &process);
    BOOL cloaked = FALSE; // suspended UWP apps and virtual-desktop windows are "cloaked"
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
    w.ignorable = process == GetCurrentProcessId() || cloaked || isShellSurface(hwnd);
    w.minimized = IsIconic(hwnd);
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    w.maximizedWithTitleBar = IsZoomed(hwnd) && (style & WS_CAPTION) == WS_CAPTION;

    RECT bounds{};
    GetWindowRect(hwnd, &bounds);
    w.bounds = toRect(bounds);
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{ sizeof info };
    GetMonitorInfoW(monitor, &info);
    w.monitor = toRect(info.rcMonitor);
    w.onIslandMonitor = monitor == MonitorFromWindow(g_island, MONITOR_DEFAULTTOPRIMARY);
    return w;
}

bool exclusiveFullscreenRunning()
{
    QUERY_USER_NOTIFICATION_STATE state{};
    return SUCCEEDED(SHQueryUserNotificationState(&state)) && state == QUNS_RUNNING_D3D_FULL_SCREEN;
}

void CALLBACK onRecheck(HWND, UINT, UINT_PTR, DWORD);

// Hidden, check again every fullscreenRecheck until shown (decision 059).
void recheckWhileHidden(bool hidden)
{
    if (hidden && !g_recheck) g_recheck = SetTimer(nullptr, 0, static_cast<UINT>(tokens().fullscreenRecheck.count()), onRecheck);
    if (!hidden && g_recheck) {
        KillTimer(nullptr, g_recheck);
        g_recheck = 0;
    }
}

void evaluate()
{
    HWND foreground = GetForegroundWindow();
    bool exclusive = exclusiveFullscreenRunning();
    bool hide = shouldHideForFullscreen(describe(foreground), exclusive);
    if (hide == g_hidden) return;
    if (hide) // which app, and how: a user's log then says what hid the island
        logLine("fullscreen app detected: hiding (%ls%s)", foreground ? describeWindow(foreground).c_str() : L"no window",
                exclusive ? ", exclusive fullscreen" : "");
    else logLine("fullscreen app gone: showing");
    g_hidden = hide;
    recheckWhileHidden(hide);
    g_onChange(hide);
}

void CALLBACK onRecheck(HWND, UINT, UINT_PTR, DWORD) { evaluate(); }

void CALLBACK onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG child, DWORD, DWORD);

// Moves/resizes only matter for the foreground app (entering/leaving F11,
// resizing to fill the screen), so the location hook follows its process:
// mouse movement and other apps' windows never wake us.
void followForegroundProcess(HWND foreground)
{
    DWORD process = 0;
    GetWindowThreadProcessId(foreground, &process);
    if (process == g_locationProcess) return;
    if (g_locationHook) UnhookWinEvent(g_locationHook);
    g_locationProcess = process;
    g_locationHook = process ? SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, onWinEvent,
                                               process, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS)
                             : nullptr;
}

void CALLBACK onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG child, DWORD, DWORD)
{
    if (event == EVENT_OBJECT_LOCATIONCHANGE) {
        // Only the foreground window itself, not its carets, cursors or children.
        if (object != OBJID_WINDOW || child != CHILDID_SELF || hwnd != GetForegroundWindow()) return;
    } else {
        followForegroundProcess(GetForegroundWindow());
    }
    evaluate();
}

} // namespace

void watchFullscreen(HWND island, std::function<void(bool fullscreen)> onChange)
{
    g_island = island;
    g_onChange = std::move(onChange);

    // Out-of-context hooks are delivered through this thread's message loop.
    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr, onWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);

    followForegroundProcess(GetForegroundWindow());
    g_hidden = false;
    evaluate(); // started under a fullscreen app: hidden from the start
}

} // namespace cup::platform
