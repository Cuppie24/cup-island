#include "providers/media/app_activation.h"

#include "platform/log.h"
#include "providers/media/browser_tabs.h"
#include "platform/win.h"

#include <dwmapi.h>
#include <propsys.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <tlhelp32.h>

#include <cwctype>
#include <iterator>
#include <thread>
#include <vector>

namespace cup {
namespace {

// PKEY_AppUserModel_ID, spelled out so no extra GUID library is needed.
constexpr PROPERTYKEY kAppUserModelId{ { 0x9F4C2855, 0x9F79, 0x4B39, { 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 } }, 5 };

std::wstring lower(std::wstring s)
{
    for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

// "C:\…\Spotify.exe", "Spotify.exe", "Chrome", "MSEdge" -> "spotify", "chrome", "msedge".
// Desktop apps' session ids are usually their exe name, with or without ".exe".
std::wstring exeStem(std::wstring path)
{
    path = lower(std::move(path));
    if (auto slash = path.find_last_of(L"\\/"); slash != std::wstring::npos) path.erase(0, slash + 1);
    if (path.size() > 4 && path.ends_with(L".exe")) path.resize(path.size() - 4);
    return path;
}

std::wstring processPath(DWORD pid)
{
    std::wstring path;
    if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t buffer[MAX_PATH * 2];
        DWORD size = static_cast<DWORD>(std::size(buffer));
        if (QueryFullProcessImageNameW(process, 0, buffer, &size)) path.assign(buffer, size);
        CloseHandle(process);
    }
    return path;
}

// The app id a window carries for taskbar grouping, if it set one.
std::wstring windowAppId(HWND hwnd)
{
    winrt::com_ptr<IPropertyStore> store;
    if (FAILED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(store.put())))) return {};
    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring id;
    if (SUCCEEDED(store->GetValue(kAppUserModelId, &value)) && value.vt == VT_LPWSTR && value.pwszVal) id = value.pwszVal;
    PropVariantClear(&value);
    return id;
}

std::wstring windowTitle(HWND hwnd)
{
    std::wstring title(static_cast<size_t>(GetWindowTextLengthW(hwnd)) + 1, L'\0');
    title.resize(static_cast<size_t>(GetWindowTextW(hwnd, title.data(), static_cast<int>(title.size()))));
    return title;
}

// What a person would call "the app's window": a visible, titled, top-level
// window that isn't a tool window or cloaked (hidden UWP frames, other desktops).
bool isAppWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) || GetWindowTextLengthW(hwnd) == 0) return false;
    if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return false;
    BOOL cloaked = FALSE;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
    return !cloaked;
}

struct Search {
    std::wstring appId; // lower case
    std::wstring stem;
    std::wstring title;
    HWND best = nullptr;
    bool bestShowsTitle = false;
    std::vector<HWND> windows; // every match, most recently used first
};

BOOL CALLBACK visitWindow(HWND hwnd, LPARAM param)
{
    auto& s = *reinterpret_cast<Search*>(param);
    if (!isAppWindow(hwnd)) return TRUE;
    bool match = lower(windowAppId(hwnd)) == s.appId;
    if (!match && !s.stem.empty()) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        match = exeStem(processPath(pid)) == s.stem;
    }
    if (!match) return TRUE;
    s.windows.push_back(hwnd);
    // Windows come in z-order, so the first match is the most recently used;
    // a later one showing the media's title (the playing tab is active there) beats it.
    bool showsTitle = !s.title.empty() && windowTitle(hwnd).find(s.title) != std::wstring::npos;
    if (!s.best || (showsTitle && !s.bestShowsTitle)) {
        s.best = hwnd;
        s.bestShowsTitle = showsTitle;
    }
    return TRUE;
}

void bringToFront(HWND hwnd)
{
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd); // allowed: the user just clicked our window
}

// Store apps: Windows activates the running instance (or starts it).
bool activatePackaged(const std::wstring& appId)
{
    winrt::com_ptr<IApplicationActivationManager> manager;
    if (FAILED(CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(manager.put()))))
        return false;
    DWORD pid = 0;
    return SUCCEEDED(manager->ActivateApplication(appId.c_str(), nullptr, AO_NONE, &pid));
}

// A running process of the app with no window (Spotify closed to the tray).
std::wstring runningExe(const std::wstring& stem)
{
    std::wstring path;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return path;
    PROCESSENTRY32W entry{ sizeof(entry) };
    for (BOOL more = Process32FirstW(snapshot, &entry); more && path.empty(); more = Process32NextW(snapshot, &entry))
        if (exeStem(entry.szExeFile) == stem) path = processPath(entry.th32ProcessID);
    CloseHandle(snapshot);
    return path;
}

bool openApp(const std::wstring& appId, const std::wstring& title)
{
    // Store app ids look like "Package_hash!App"; desktop apps' name their exe.
    bool packaged = appId.find(L'!') != std::wstring::npos;
    Search search{ lower(appId), packaged ? L"" : exeStem(appId), title };
    EnumWindows(visitWindow, reinterpret_cast<LPARAM>(&search));
    if (search.best) {
        HWND target = search.best;
        // No window shows the media, so it isn't an active tab anywhere: look
        // through the tabs. (Spotify & co. show the track in their title and skip this.)
        if (!search.bestShowsTitle)
            if (HWND tabWindow = selectMediaTab(search.windows, winrt::to_string(title))) target = tabWindow;
        bringToFront(target);
        return true;
    }
    if (packaged) return activatePackaged(appId);

    // No window: start the exe again. Single-instance apps (Spotify) just show
    // their existing window. Not done blindly by app id: a browser would open
    // a new window.
    if (std::wstring exe = runningExe(search.stem); !exe.empty()) {
        auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        return result > 32;
    }
    platform::logLine("media: no window found for %ls", appId.c_str());
    return false;
}

} // namespace

void openSourceApp(const std::wstring& appId, const std::wstring& title)
{
    // Off the UI thread: UI Automation calls into the other app and can take
    // a few hundred ms. The foreground right from the click is per process,
    // so this thread may still use it.
    std::thread([appId, title] {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            bool opened = openApp(appId, title);
            platform::logLine("media: open %ls %s", appId.c_str(), opened ? "ok" : "failed");
            winrt::uninit_apartment();
        } catch (...) { // an escaping exception would end the app
            platform::logLine("media: opening %ls failed with an exception", appId.c_str());
        }
    }).detach();
}

} // namespace cup
