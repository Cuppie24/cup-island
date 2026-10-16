#include "platform/outside_clicks.h"

#include "platform/log.h"

namespace cup::platform {
namespace {

HHOOK g_hook = nullptr;
HWND g_island = nullptr;

// Called for every mouse event on the system: only compares and posts,
// or Windows drops a slow hook (LowLevelHooksTimeout).
LRESULT CALLBACK onMouse(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION && (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN)) {
        const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        RECT island{};
        if (g_island && GetWindowRect(g_island, &island) && !PtInRect(&island, mouse->pt)) PostMessageW(g_island, outsideClickMessage(), 0, 0);
    }
    return CallNextHookEx(g_hook, code, message, data);
}

} // namespace

UINT outsideClickMessage()
{
    static const UINT message = RegisterWindowMessageW(L"CupIsland.ClickedOutside");
    return message;
}

void watchOutsideClicks(HWND island, bool on)
{
    if (on == (g_hook != nullptr)) return;
    if (on) {
        g_island = island;
        g_hook = SetWindowsHookExW(WH_MOUSE_LL, onMouse, GetModuleHandleW(nullptr), 0);
        if (!g_hook) logLine("can't watch for clicks outside the island (0x%08lx)", GetLastError());
    } else {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}

} // namespace cup::platform
