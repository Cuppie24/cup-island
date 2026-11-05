#include "providers/search/windows_apps.h"

#include "platform/log.h"
#include "platform/win.h"

#include <ole2.h>
#include <propsys.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <mutex>
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

// PKEY_Link_TargetParsingPath, spelled out so no extra GUID library is needed (as app_activation.cpp).
constexpr PROPERTYKEY kLinkTarget{ { 0xB9B4B3FC, 0x2B51, 0x4A42, { 0xB5, 0xD8, 0x32, 0x41, 0x46, 0xAF, 0xCF, 0x25 } }, 2 };

// A Store app's AppUserModelID is "<package family>!<app>": its family (decision 086). A desktop app's has no '!'.
std::string packageOf(const std::wstring& id)
{
    std::size_t bang = id.find(L'!');
    return bang == std::wstring::npos || bang == 0 ? std::string() : utf8(id.substr(0, bang).c_str());
}

// A desktop program's file (decision 086): its shortcut's target; else its
// AppUserModelID when that is a path, written from a known folder
// ("{6D809377-...}\\App\\app.exe") or in full. Empty: not known (a web link, a Store app).
std::string programFile(IShellItem* item, const std::wstring& id)
{
    std::wstring path;
    winrt::com_ptr<IShellItem2> more;
    PWSTR target = nullptr;
    if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(more.put()))) && SUCCEEDED(more->GetString(kLinkTarget, &target)) && target) path = target;
    CoTaskMemFree(target);
    if (path.empty() && id.size() > 2 && id[0] == L'{') {
        std::size_t close = id.find(L'}');
        KNOWNFOLDERID folder{};
        PWSTR base = nullptr;
        if (close != std::wstring::npos && SUCCEEDED(CLSIDFromString(id.substr(0, close + 1).c_str(), &folder)) &&
            SUCCEEDED(SHGetKnownFolderPath(folder, KF_FLAG_DEFAULT, nullptr, &base)))
            path = std::wstring(base) + id.substr(close + 1);
        CoTaskMemFree(base);
    } else if (path.empty() && id.size() > 2 && id[1] == L':' && id[2] == L'\\') {
        path = id;
    }
    if (path.find(L"://") != std::wstring::npos) return {}; // a link to a website, not a file
    return utf8(path.c_str());
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
        if (SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &name)) && SUCCEEDED(item->GetDisplayName(how, &parsing))) {
            out.push_back({ kind, utf8(name), utf8((prefix + parsing).c_str()), {}, nullptr });
            if (kind == Launchable::Kind::App) { // where it lives, for its menu (decision 086)
                out.back().package = packageOf(parsing);
                if (out.back().package.empty()) out.back().location = programFile(item.get(), parsing);
            }
        }
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

// One item's icon as the shell draws it, `side` pixels square, as
// premultiplied BGRA (what Direct2D draws). Nothing if the shell has none.
std::shared_ptr<const ImageData> readIcon(const std::wstring& target, int side)
{
    winrt::com_ptr<IShellItemImageFactory> factory;
    if (FAILED(SHCreateItemFromParsingName(target.c_str(), nullptr, IID_PPV_ARGS(factory.put())))) return nullptr;
    HBITMAP bitmap = nullptr;
    if (FAILED(factory->GetImage(SIZE{ side, side }, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &bitmap)) || !bitmap) return nullptr;
    BITMAP info{};
    GetObjectW(bitmap, sizeof info, &info);
    auto image = std::make_shared<ImageData>();
    image->width = info.bmWidth;
    image->height = std::abs(info.bmHeight);
    image->bgra.resize(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height) * 4);
    BITMAPINFO want{};
    want.bmiHeader = { sizeof(BITMAPINFOHEADER), image->width, -image->height, 1, 32, BI_RGB };
    HDC dc = GetDC(nullptr);
    int lines = image->bgra.empty() ? 0 : GetDIBits(dc, bitmap, 0, static_cast<UINT>(image->height), image->bgra.data(), &want, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    DeleteObject(bitmap);
    if (lines != image->height) return nullptr;
    // The shell's 32-bit icons carry premultiplied alpha; an old one with
    // none at all (every alpha 0) is opaque, not invisible.
    bool anyAlpha = false;
    for (std::size_t i = 3; i < image->bgra.size() && !anyAlpha; i += 4) anyAlpha = image->bgra[i] != 0;
    if (!anyAlpha)
        for (std::size_t i = 3; i < image->bgra.size(); i += 4) image->bgra[i] = 255;
    return image;
}

// Icons are read one at a time on a thread of their own (the shell wants a
// single-threaded apartment, and a slow drive must not hold up the island).
struct IconJob {
    std::wstring target;
    int side = 0;
    winrt::Windows::System::DispatcherQueue queue{ nullptr };
    std::function<void(std::shared_ptr<const ImageData>)> done;
};
std::mutex g_iconLock;
std::condition_variable g_iconWake;
std::deque<IconJob> g_iconJobs;

void iconWorker()
{
    (void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // never left: the thread lives as long as the app
    for (;;) {
        IconJob job;
        {
            std::unique_lock lock(g_iconLock);
            g_iconWake.wait(lock, [] { return !g_iconJobs.empty(); });
            job = std::move(g_iconJobs.front());
            g_iconJobs.pop_front();
        }
        auto icon = readIcon(job.target, job.side);
        if (job.queue) job.queue.TryEnqueue([icon, done = std::move(job.done)] { done(icon); });
    }
}

} // namespace

void loadIcon(const std::string& target, int side, std::function<void(std::shared_ptr<const ImageData>)> done)
{
    static std::once_flag started;
    std::call_once(started, [] { std::thread(iconWorker).detach(); }); // lives as long as the app
    {
        std::lock_guard lock(g_iconLock);
        g_iconJobs.push_back({ wide(target), side, winrt::Windows::System::DispatcherQueue::GetForCurrentThread(), std::move(done) });
    }
    g_iconWake.notify_one();
}

bool pathExists(const std::string& path)
{
    return GetFileAttributesW(wide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool revealPath(const std::string& path)
{
    PIDLIST_ABSOLUTE item = nullptr;
    if (FAILED(SHParseDisplayName(wide(path).c_str(), nullptr, &item, 0, nullptr))) return false;
    bool ok = SUCCEEDED(SHOpenFolderAndSelectItems(item, 0, nullptr, 0)); // its folder, it selected
    CoTaskMemFree(item);
    if (!ok) platform::logLine("search: couldn't show %s in its folder", path.c_str());
    return ok;
}

std::string userFolder()
{
    PWSTR path = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &path))) out = utf8(path);
    CoTaskMemFree(path);
    return out;
}

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

// The shell item's PIDL, run with `verb` ("runas"): an app through its shortcut (its arguments kept), or a file.
bool runVerb(const std::string& target, const wchar_t* verb)
{
    winrt::com_ptr<IShellItem> shellItem;
    if (FAILED(SHCreateItemFromParsingName(wide(target).c_str(), nullptr, IID_PPV_ARGS(shellItem.put())))) return false;
    PIDLIST_ABSOLUTE list = nullptr;
    if (FAILED(SHGetIDListFromObject(shellItem.get(), &list))) return false;
    SHELLEXECUTEINFOW info{ sizeof info };
    info.fMask = SEE_MASK_IDLIST | SEE_MASK_FLAG_NO_UI;
    info.lpIDList = list;
    info.lpVerb = verb;
    info.nShow = SW_SHOWNORMAL;
    bool ok = ShellExecuteExW(&info) != FALSE;
    CoTaskMemFree(list);
    // The user saying No to the consent prompt is ERROR_CANCELLED: not worth a line in the log.
    if (!ok && GetLastError() != ERROR_CANCELLED) platform::logLine("search: %s didn't start as admin (%lu)", target.c_str(), GetLastError());
    return ok;
}

bool runAsAdmin(const std::string& target) { return runVerb(target, L"runas"); }

bool openTerminal(const std::string& folder)
{
    // Windows Terminal (an app alias on PATH where it's installed; Windows 10 has
    // none by default), else PowerShell, which every Windows since 7 has (decision 015).
    std::wstring dir = wide(folder);
    std::wstring args = L"-d \"" + dir + (dir.size() > 0 && dir.back() == L'\\' ? L"\\" : L"") + L"\"";
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"wt.exe", args.c_str(), nullptr, SW_SHOWNORMAL)) > 32) return true;
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"powershell.exe", L"-NoExit", dir.c_str(), SW_SHOWNORMAL)) > 32;
}

bool showProperties(const std::string& path)
{
    return SHObjectProperties(nullptr, SHOP_FILEPATH, wide(path).c_str(), nullptr) != FALSE; // Explorer's window, its own thread
}

bool copyFileToClipboard(const std::string& path)
{
    // The shell item's own data object, as Explorer's Copy puts on the clipboard, marked as a copy (never a move).
    if (FAILED(OleInitialize(nullptr))) return false; // the clipboard's OLE side; counted, so a second call is harmless
    winrt::com_ptr<IShellItem> item;
    winrt::com_ptr<IDataObject> data;
    bool ok = SUCCEEDED(SHCreateItemFromParsingName(wide(path).c_str(), nullptr, IID_PPV_ARGS(item.put()))) &&
              SUCCEEDED(item->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(data.put())));
    if (ok) {
        FORMATETC format{ static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT)), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium{ TYMED_HGLOBAL };
        medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
        if (medium.hGlobal) {
            if (auto* effect = static_cast<DWORD*>(GlobalLock(medium.hGlobal))) {
                *effect = DROPEFFECT_COPY;
                GlobalUnlock(medium.hGlobal);
            }
            if (FAILED(data->SetData(&format, &medium, TRUE))) ReleaseStgMedium(&medium); // TRUE: it owns the memory now
        }
        ok = SUCCEEDED(OleSetClipboard(data.get())) && SUCCEEDED(OleFlushClipboard()); // flushed: it stays after the island lets go
    }
    if (!ok) platform::logLine("search: couldn't copy %s", path.c_str());
    OleUninitialize();
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
