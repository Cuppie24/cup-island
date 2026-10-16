#include "platform/drag_source.h"

#include "platform/log.h"

#include <ole2.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace cup::platform {
namespace {

bool g_out = false;

std::wstring wide(const std::string& utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

} // namespace

bool draggingOut() { return g_out; }

void dragFilesOut(HWND window, const std::vector<std::string>& paths)
{
    std::vector<PIDLIST_ABSOLUTE> items;
    for (const std::string& path : paths) {
        PIDLIST_ABSOLUTE item = nullptr;
        if (SUCCEEDED(SHParseDisplayName(wide(path).c_str(), nullptr, &item, 0, nullptr)) && item) items.push_back(item);
    }
    if (!items.empty()) {
        // The shell's own data object for them, wherever they are: Explorer
        // takes it as its own drags (files, a drag image, shortcuts on Alt).
        winrt::com_ptr<IShellItemArray> array;
        winrt::com_ptr<IDataObject> data;
        if (SUCCEEDED(SHCreateShellItemArrayFromIDLists(static_cast<UINT>(items.size()), const_cast<PCIDLIST_ABSOLUTE_ARRAY>(items.data()), array.put())) &&
            SUCCEEDED(array->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(data.put())))) {
            g_out = true;
            DWORD effect = DROPEFFECT_NONE;
            ULONGLONG began = GetTickCount64();
            HRESULT hr = SHDoDragDrop(window, data.get(), nullptr, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
            g_out = false;
            if (FAILED(hr)) logLine("drag out failed (0x%08lx)", static_cast<unsigned long>(hr));
            // Its end, to line up with what follows it (play/pause once showed late after a drag out).
            else logLine("drag out ended (%s, %llu ms)", hr == DRAGDROP_S_DROP ? (effect & DROPEFFECT_LINK ? "linked" : "copied") : "cancelled",
                         static_cast<unsigned long long>(GetTickCount64() - began));
        }
    }
    for (PIDLIST_ABSOLUTE item : items) CoTaskMemFree(item);
}

} // namespace cup::platform
