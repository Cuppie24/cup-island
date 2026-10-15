#include "platform/stay_on_top.h"

#include "platform/log.h"
#include "platform/window_name.h"

#include <dwmapi.h>

#include <string>

namespace cup::platform {
namespace {

std::vector<HWND> g_windows;
std::wstring g_lastCulprit; // logged once per culprit, not on every switch

// The first visible window above `island` in the z-order that overlaps it.
HWND coveringWindow(HWND island)
{
    RECT ours{};
    GetWindowRect(island, &ours);
    for (HWND above = GetWindow(island, GW_HWNDPREV); above; above = GetWindow(above, GW_HWNDPREV)) {
        if (!IsWindowVisible(above)) continue;
        BOOL cloaked = FALSE;
        DwmGetWindowAttribute(above, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
        RECT theirs{}, overlap{};
        GetWindowRect(above, &theirs);
        if (!cloaked && IntersectRect(&overlap, &ours, &theirs)) return above;
    }
    return nullptr;
}

void raise()
{
    HWND top = g_windows.back();
    if (!IsWindow(top)) return;
    bool lostFlag = (GetWindowLongW(top, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0;
    HWND cover = coveringWindow(top);
    if (lostFlag || cover) {
        std::wstring culprit = lostFlag ? L"(lost the topmost flag)" : describeWindow(cover);
        if (culprit != g_lastCulprit) logLine("island was covered by %ls; raising it", culprit.c_str());
        g_lastCulprit = culprit;
    }
    // Bottom to top: each lands at the top of the topmost band, the last one highest.
    for (HWND hwnd : g_windows)
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void CALLBACK onForeground(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) { raise(); }

} // namespace

void stayOnTop(std::vector<HWND> windows)
{
    g_windows = std::move(windows);
    if (g_windows.empty()) return;
    // Out of context: delivered through this (UI) thread's message loop.
    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onForeground, 0, 0, WINEVENT_OUTOFCONTEXT);
}

} // namespace cup::platform
