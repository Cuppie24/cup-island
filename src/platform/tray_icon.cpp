#include "platform/tray_icon.h"

#include "platform/autostart.h"

#include <shellapi.h>

#include <utility>

namespace cup::platform {
namespace {

constexpr wchar_t kClassName[] = L"CupIslandTray";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kStartWithWindowsCommand = 1;
constexpr UINT kExitCommand = 2;

HWND g_hwnd{};
UINT g_taskbarCreated = 0; // broadcast when Explorer (re)starts: every tray icon must be re-added
NOTIFYICONDATAW g_icon{ sizeof(g_icon) };
std::function<void()> g_onExit;

void addIcon()
{
    Shell_NotifyIconW(NIM_ADD, &g_icon);
    Shell_NotifyIconW(NIM_SETVERSION, &g_icon); // version 4: right-click arrives as WM_CONTEXTMENU
}

void showMenu()
{
    HMENU menu = CreatePopupMenu();
    bool startup = startsWithWindows(); // read each time: the user may have changed it in Task Manager
    AppendMenuW(menu, MF_STRING | (startup ? MF_CHECKED : MF_UNCHECKED), kStartWithWindowsCommand, L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kExitCommand, L"Exit");
    POINT at{};
    GetCursorPos(&at);
    // Without this the menu doesn't close when clicking elsewhere (documented TrackPopupMenu quirk).
    SetForegroundWindow(g_hwnd);
    UINT picked = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, at.x, at.y, 0, g_hwnd, nullptr);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (picked == kStartWithWindowsCommand) setStartsWithWindows(!startup);
    if (picked == kExitCommand && g_onExit) g_onExit();
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == kTrayMessage) {
        if (LOWORD(lParam) == WM_CONTEXTMENU) showMenu();
        return 0;
    }
    if (g_taskbarCreated && message == g_taskbarCreated) {
        addIcon();
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

void createTrayIcon(HINSTANCE instance, std::function<void()> onExit)
{
    g_onExit = std::move(onExit);
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW windowClass{ sizeof(windowClass) };
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kClassName;
    RegisterClassExW(&windowClass);
    // A hidden top-level window, not a message-only one: only top-level windows get TaskbarCreated.
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"Cup Island Tray", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);

    HICON icon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    g_icon.hWnd = g_hwnd;
    g_icon.uID = 1;
    g_icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_icon.uCallbackMessage = kTrayMessage;
    g_icon.hIcon = icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
    g_icon.uVersion = NOTIFYICON_VERSION_4;
    wcscpy_s(g_icon.szTip, L"Cup Island");
    addIcon();
}

void removeTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &g_icon);
    DestroyWindow(g_hwnd);
}

} // namespace cup::platform
