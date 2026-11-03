#include "providers/search/windows_apps.h"

#include "platform/log.h"
#include "platform/win.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <thread>

namespace cup {
namespace {

std::string utf8(const wchar_t* text)
{
    if (!text || !*text) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring wide(const std::string& text)
{
    if (text.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}

// Every item of a shell folder: its name as shown, and the path to reach it
// again (`prefix` + its parsing name, or its full parsing path).
void addItems(IShellItem* folder, Launchable::Kind kind, const std::wstring& prefix, std::vector<Launchable>& out)
{
    winrt::com_ptr<IEnumShellItems> items;
    if (FAILED(folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(items.put())))) return;
    for (winrt::com_ptr<IShellItem> item; items->Next(1, item.put(), nullptr) == S_OK; item = nullptr) {
        PWSTR name = nullptr, parsing = nullptr;
        // An app's parsing name inside the Applications folder is its
        // AppUserModelID (or a path); a Control Panel item's is a full path.
        SIGDN how = prefix.empty() ? SIGDN_DESKTOPABSOLUTEPARSING : SIGDN_PARENTRELATIVEPARSING;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &name)) && SUCCEEDED(item->GetDisplayName(how, &parsing)))
            out.push_back({ kind, utf8(name), utf8((prefix + parsing).c_str()), {}, nullptr });
        CoTaskMemFree(name);
        CoTaskMemFree(parsing);
    }
}

std::vector<Launchable> readApps()
{
    std::vector<Launchable> out;
    winrt::com_ptr<IShellItem> apps;
    if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(apps.put()))))
        addItems(apps.get(), Launchable::Kind::App, L"shell:AppsFolder\\", out);
    std::size_t appCount = out.size();
    // "All Control Panel Items": two shell ids are in use for it across
    // versions; the first that opens is read.
    for (const wchar_t* path : { L"shell:::{21EC2020-3AEA-1069-A2DD-08002B30309D}", L"shell:::{26EE0668-A00A-44D7-9371-BEB064C98683}\\0" }) {
        winrt::com_ptr<IShellItem> panel;
        if (FAILED(SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(panel.put())))) continue;
        addItems(panel.get(), Launchable::Kind::ControlPanel, L"", out);
        if (out.size() > appCount) break;
    }
    platform::logLine("search: %zu apps, %zu Control Panel items", appCount, out.size() - appCount);
    return out;
}

} // namespace

void loadWindowsApps(std::function<void(std::vector<Launchable>)> done)
{
    auto queue = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    std::thread([queue, done = std::move(done)] {
        // Its own single-threaded apartment: the shell's folders expect one,
        // and a pool thread is already in the multithreaded one.
        bool com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
        std::vector<Launchable> apps = readApps();
        if (com) CoUninitialize();
        if (queue) queue.TryEnqueue([apps = std::move(apps), done] { done(apps); });
    }).detach();
}

bool launchItem(const Launchable& item)
{
    if (item.kind == Launchable::Kind::Setting) {
        std::wstring link = wide(item.target);
        return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    }
    // Through the shell item: Store apps have no exe to run, and a desktop
    // app's shortcut keeps its arguments and working folder this way.
    winrt::com_ptr<IShellItem> shellItem;
    if (FAILED(SHCreateItemFromParsingName(wide(item.target).c_str(), nullptr, IID_PPV_ARGS(shellItem.put())))) {
        platform::logLine("search: %s is gone", item.name.c_str());
        return false;
    }
    PIDLIST_ABSOLUTE list = nullptr;
    if (FAILED(SHGetIDListFromObject(shellItem.get(), &list))) return false;
    SHELLEXECUTEINFOW info{ sizeof info };
    info.fMask = SEE_MASK_IDLIST | SEE_MASK_FLAG_NO_UI;
    info.lpIDList = list;
    info.nShow = SW_SHOWNORMAL;
    bool ok = ShellExecuteExW(&info) != FALSE;
    CoTaskMemFree(list);
    if (!ok) platform::logLine("search: %s didn't start (%lu)", item.name.c_str(), GetLastError());
    return ok;
}

bool openPath(const std::string& path)
{
    std::wstring file = wide(path);
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, nullptr, file.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

int windowsBuild()
{
    // RtlGetVersion, by name (decision 015): GetVersionEx lies to apps without a manifest entry.
    using Get = LONG(WINAPI*)(OSVERSIONINFOW*);
    auto get = reinterpret_cast<Get>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW v{ sizeof v };
    if (get && get(&v) == 0) return static_cast<int>(v.dwBuildNumber);
    return 17763;
}

bool russianUi() { return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN; }

} // namespace cup
