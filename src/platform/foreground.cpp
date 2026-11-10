#include "platform/foreground.h"

#include "core/tokens.h"
#include "platform/log.h"

#include <cwctype>
#include <string>

namespace cup::platform {
namespace {

HWND g_target = nullptr;
std::size_t g_triesLeft = 0;
bool g_escapeSent = false; // one Esc per shortcut: a second would reach the app behind
UINT_PTR g_timer = 0;

std::wstring exeName(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    if (!ok) return {};
    std::wstring name = path;
    name = name.substr(name.find_last_of(L'\\') + 1);
    for (auto& c : name) c = static_cast<wchar_t>(std::towlower(c));
    return name;
}

// Start and Windows search: flyouts that keep the foreground from anyone
// else until they close. Their hosts, Windows 10 1809 to Windows 11.
bool isShellFlyout(HWND hwnd)
{
    static const wchar_t* const kHosts[] = {
        L"startmenuexperiencehost.exe", // Start, Windows 10 1903+ and 11
        L"shellexperiencehost.exe",     // Start on 1809, the notification center
        L"searchhost.exe", L"searchapp.exe", L"searchui.exe", // search, 11 / 10 2004+ / 1809
    };
    if (!hwnd) return false;
    std::wstring name = exeName(hwnd);
    for (const wchar_t* host : kHosts)
        if (name == host) return true;
    return false;
}

// A modifier still held would turn Esc into another shortcut: Ctrl+Esc
// opens Start, Alt+Esc switches windows, Win+Esc closes the magnifier.
bool modifierHeld()
{
    for (int vk : { VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN })
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    return false;
}

void tap(WORD vk)
{
    INPUT keys[2] = {};
    keys[0].type = keys[1].type = INPUT_KEYBOARD;
    keys[0].ki.wVk = keys[1].ki.wVk = vk;
    keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, keys, sizeof(INPUT));
}

bool inFront(HWND hwnd) { return GetForegroundWindow() == hwnd; }

// The ways in, mildest first. Each is checked by what's really in front:
// SetForegroundWindow can say yes while the window only flashes.
bool tryFront(HWND hwnd)
{
    if (SetForegroundWindow(hwnd) && inFront(hwnd)) return true;
    // Refused: a harmless Alt tap lifts Windows' foreground lock, as
    // launchers such as PowerToys do.
    tap(VK_MENU);
    if (SetForegroundWindow(hwnd) && inFront(hwnd)) return true;
    // Still refused: share input with the window in front for a moment, so
    // to Windows the request comes from it.
    HWND front = GetForegroundWindow();
    DWORD frontThread = front ? GetWindowThreadProcessId(front, nullptr) : 0, self = GetCurrentThreadId();
    if (frontThread && frontThread != self && AttachThreadInput(self, frontThread, TRUE)) {
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
        AttachThreadInput(self, frontThread, FALSE);
    }
    return inFront(hwnd);
}

void stopTrying()
{
    if (g_timer) KillTimer(nullptr, g_timer);
    g_timer = 0;
    g_triesLeft = 0;
}

// One attempt: Start or search in front is closed first (once, when no
// modifier is held), and the island comes forward once it has gone.
bool attempt()
{
    if (!IsWindow(g_target)) return true; // gone: nothing left to bring
    HWND front = GetForegroundWindow();
    if (front == g_target) return true;
    if (isShellFlyout(front)) {
        if (!g_escapeSent && !modifierHeld()) {
            tap(VK_ESCAPE);
            g_escapeSent = true;
        }
        return false; // closing takes a moment: the next tick tries
    }
    return tryFront(g_target);
}

void CALLBACK onRetry(HWND, UINT, UINT_PTR, DWORD)
{
    bool done = attempt();
    if (!done && --g_triesLeft > 0) return;
    if (done) SetFocus(g_target);
    else logLine("shortcut: the island couldn't come to the front (%ls in front)", exeName(GetForegroundWindow()).c_str());
    stopTrying();
}

} // namespace

void bringToFront(HWND hwnd)
{
    stopTrying();
    g_target = hwnd;
    g_escapeSent = false;
    if (attempt()) {
        SetFocus(hwnd);
        return;
    }
    g_triesLeft = tokens().foregroundRetries;
    if (g_triesLeft == 0) return;
    g_timer = SetTimer(nullptr, 0, static_cast<UINT>(tokens().foregroundRetry.count()), onRetry);
}

} // namespace cup::platform
