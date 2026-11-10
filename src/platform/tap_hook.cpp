#include "platform/tap_hook.h"

#include "core/double_tap.h"
#include "core/shortcuts.h"
#include "platform/log.h"

#include <atomic>
#include <optional>
#include <thread>

namespace cup::platform {
namespace {

// Only the hook's thread touches these, except g_threadId (to stop it).
HWND g_target = nullptr;
int g_id = 0;
std::optional<DoubleTap> g_detector;
std::thread g_thread;
std::atomic<DWORD> g_threadId{ 0 };

// Either side's key as the plain one: left and right Ctrl are both Ctrl.
int plainKey(DWORD vk)
{
    switch (vk) {
    case VK_LCONTROL:
    case VK_RCONTROL: return kCtrl;
    case VK_LSHIFT:
    case VK_RSHIFT: return kShift;
    case VK_LMENU:
    case VK_RMENU: return kAlt;
    default: return static_cast<int>(vk);
    }
}

LRESULT CALLBACK onKey(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION && g_detector) {
        auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
        if ((down || up) && g_detector->feed(plainKey(key->vkCode), down, key->time))
            PostMessageW(g_target, WM_HOTKEY, static_cast<WPARAM>(g_id), 0);
    }
    return CallNextHookEx(nullptr, code, message, data); // watched, never swallowed
}

void run(HANDLE ready, bool* ok)
{
    g_threadId = GetCurrentThreadId();
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, onKey, GetModuleHandleW(nullptr), 0);
    *ok = hook != nullptr;
    if (!hook) logLine("tap shortcut: the keyboard hook couldn't be set (%lu)", GetLastError());
    SetEvent(ready);
    if (!hook) return;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {} // the hook's callbacks come through this loop
    UnhookWindowsHookEx(hook);
}

} // namespace

bool startTapHook(HWND hwnd, int id, int key)
{
    stopTapHook();
    g_target = hwnd;
    g_id = id;
    g_detector.emplace(key);
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    bool ok = false;
    g_thread = std::thread(run, ready, &ok);
    WaitForSingleObject(ready, 2000);
    CloseHandle(ready);
    if (!ok) stopTapHook();
    return ok;
}

void stopTapHook()
{
    if (!g_thread.joinable()) return;
    if (DWORD id = g_threadId.load()) PostThreadMessageW(id, WM_QUIT, 0, 0);
    g_thread.join();
    g_threadId = 0;
    g_detector.reset();
}

} // namespace cup::platform
