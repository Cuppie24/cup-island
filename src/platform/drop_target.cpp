#include "platform/drop_target.h"

#include "platform/log.h"
#include "platform/text_input.h"

#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace cup::platform {
namespace {

// What the drag holds, read once when it enters.
struct Held {
    std::vector<DroppedItem> items; // paths, or the text or link
    DragSummary summary;            // the same, as providers see it
};

std::wstring wide(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

std::string nameOf(const std::wstring& path)
{
    std::size_t slash = path.find_last_of(L"\\/");
    return toUtf8(slash == std::wstring::npos ? path : path.substr(slash + 1));
}

// One format as text, if the drag has it (TYMED_HGLOBAL, UTF-16).
std::optional<std::wstring> textIn(IDataObject* data, CLIPFORMAT format)
{
    FORMATETC fmt{ format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium{};
    if (FAILED(data->GetData(&fmt, &medium))) return std::nullopt;
    std::optional<std::wstring> out;
    if (auto* text = static_cast<const wchar_t*>(GlobalLock(medium.hGlobal))) {
        SIZE_T chars = GlobalSize(medium.hGlobal) / sizeof(wchar_t);
        out = std::wstring(text, wcsnlen(text, chars));
        GlobalUnlock(medium.hGlobal);
    }
    ReleaseStgMedium(&medium);
    return out;
}

std::vector<std::wstring> hdropPaths(IDataObject* data)
{
    std::vector<std::wstring> paths;
    FORMATETC fmt{ CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium{};
    if (FAILED(data->GetData(&fmt, &medium))) return paths;
    auto drop = static_cast<HDROP>(medium.hGlobal);
    UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        UINT length = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring path(length + 1, L'\0');
        DragQueryFileW(drop, i, path.data(), length + 1);
        path.resize(length);
        paths.push_back(path);
    }
    ReleaseStgMedium(&medium);
    return paths;
}

// Shell items with a place on disk (a library's files); the rest (a phone,
// Control Panel) have no path and aren't taken in this version.
std::vector<std::wstring> shellPaths(IDataObject* data)
{
    std::vector<std::wstring> paths;
    IShellItemArray* array = nullptr;
    if (FAILED(SHCreateShellItemArrayFromDataObject(data, IID_PPV_ARGS(&array)))) return paths;
    DWORD count = 0;
    array->GetCount(&count);
    for (DWORD i = 0; i < count; ++i) {
        IShellItem* item = nullptr;
        if (FAILED(array->GetItemAt(i, &item))) continue;
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            paths.push_back(path);
            CoTaskMemFree(path);
        }
        item->Release();
    }
    array->Release();
    return paths;
}

// Files first; then a link (browsers offer its text too); then text.
Held read(IDataObject* data, std::size_t maxChecked)
{
    Held held;
    std::vector<std::wstring> paths = hdropPaths(data);
    if (paths.empty()) paths = shellPaths(data);
    for (const std::wstring& path : paths) {
        DroppedItem item{ .what = { DragKind::File, nameOf(path), std::nullopt }, .path = toUtf8(path) };
        held.items.push_back(item);
    }
    if (held.items.empty()) {
        static const CLIPFORMAT url = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(CFSTR_INETURLW));
        if (auto link = textIn(data, url); link && !link->empty()) {
            std::string address = toUtf8(*link);
            held.items.push_back({ .what = { DragKind::Url, address, address.size() }, .text = address });
        } else if (auto text = textIn(data, CF_UNICODETEXT); text && !text->empty()) {
            std::string utf8 = toUtf8(*text);
            held.items.push_back({ .what = { DragKind::Text, utf8.substr(0, 40), utf8.size() }, .text = utf8 });
        }
    }
    held.summary.total = held.items.size();
    held.summary.sizesKnown = true;
    for (std::size_t i = 0; i < held.items.size() && i < maxChecked; ++i) { // a huge drag is only counted
        held.summary.items.push_back(held.items[i].what);
        if (held.items[i].what.kind == DragKind::File) held.summary.sizesKnown = false; // read off the UI thread
    }
    return held;
}

// Folder or file, and a file's size: the disk is touched here, so never on
// the UI thread while the drag moves (a network share can take seconds).
void readSizes(Held& held, std::size_t maxChecked, const std::wstring& storeFolder)
{
    std::size_t checked = 0;
    for (DroppedItem& item : held.items) {
        if (item.what.kind != DragKind::File || checked++ >= maxChecked) continue;
        WIN32_FILE_ATTRIBUTE_DATA info{};
        if (!GetFileAttributesExW(wide(item.path).c_str(), GetFileExInfoStandard, &info)) continue;
        if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) item.what.kind = DragKind::Folder;
        else item.what.bytes = (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    }
    held.summary.items.clear();
    for (std::size_t i = 0; i < held.items.size() && i < maxChecked; ++i) held.summary.items.push_back(held.items[i].what);
    ULARGE_INTEGER free{};
    if (!storeFolder.empty() && GetDiskFreeSpaceExW(storeFolder.c_str(), &free, nullptr, nullptr)) held.summary.freeBytes = free.QuadPart;
    held.summary.sizesKnown = true;
}

DWORD effectFor(DropEffect want, DWORD allowed)
{
    // Never a move: on a move the source deletes the original (decision 061).
    if (want == DropEffect::Link && (allowed & DROPEFFECT_LINK)) return DROPEFFECT_LINK;
    if (want != DropEffect::None && (allowed & DROPEFFECT_COPY)) return DROPEFFECT_COPY;
    if (want != DropEffect::None && (allowed & DROPEFFECT_LINK)) return DROPEFFECT_LINK; // we only read: any non-move says yes
    return DROPEFFECT_NONE;
}

// A plain COM object: OLE calls it on the UI thread, inside the drag
// source's loop, so each call stays short.
class Target final : public IDropTarget {
public:
    explicit Target(DropCallbacks callbacks) : callbacks_(std::move(callbacks)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
    {
        if (!out) return E_POINTER;
        if (iid == IID_IUnknown || iid == IID_IDropTarget) {
            *out = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG left = --refs_;
        if (left == 0) delete this;
        return left;
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL at, DWORD* effect) override
    {
        held_ = std::make_shared<Held>(read(data, callbacks_.maxChecked));
        ++generation_;
        if (held_->items.empty()) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }
        callbacks_.onEnter(held_->summary);
        if (!held_->summary.sizesKnown) readSizesInBackground();
        *effect = effectFor(callbacks_.onOver({ at.x, at.y }), *effect);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL at, DWORD* effect) override
    {
        *effect = held_ && !held_->items.empty() ? effectFor(callbacks_.onOver({ at.x, at.y }), *effect) : DROPEFFECT_NONE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        if (held_ && !held_->items.empty()) callbacks_.onLeave();
        held_.reset();
        ++generation_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject*, DWORD, POINTL at, DWORD* effect) override
    {
        std::shared_ptr<Held> held = std::exchange(held_, nullptr);
        ++generation_;
        if (!held || held->items.empty()) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }
        // Dropped before the sizes came: read them now, once.
        if (!held->summary.sizesKnown) readSizes(*held, callbacks_.maxChecked, callbacks_.storeFolder);
        *effect = effectFor(callbacks_.onDrop({ at.x, at.y }, held->items), *effect);
        return S_OK;
    }

private:
    void readSizesInBackground()
    {
        auto copy = std::make_shared<Held>(*held_);
        unsigned generation = generation_;
        std::size_t maxChecked = callbacks_.maxChecked;
        std::wstring store = callbacks_.storeFolder;
        auto post = callbacks_.postToUi;
        // `this` lives while the window is registered; a stale drag's answer is ignored.
        AddRef();
        std::thread([this, copy, generation, maxChecked, store, post] {
            readSizes(*copy, maxChecked, store);
            post([this, copy, generation] {
                if (generation == generation_ && held_) {
                    held_ = copy;
                    callbacks_.onSizes(held_->summary);
                }
                Release();
            });
        }).detach();
    }

    std::atomic<ULONG> refs_{ 1 };
    DropCallbacks callbacks_;
    std::shared_ptr<Held> held_;
    unsigned generation_ = 0; // UI thread only: which drag a size reading belongs to
};

} // namespace

bool registerDropTarget(HWND window, DropCallbacks callbacks)
{
    auto* target = new Target(std::move(callbacks));
    HRESULT hr = RegisterDragDrop(window, target); // holds its own reference
    target->Release();
    if (FAILED(hr)) logLine("drag and drop unavailable (RegisterDragDrop 0x%08lx)", static_cast<unsigned long>(hr));
    return SUCCEEDED(hr);
}

void revokeDropTarget(HWND window) { RevokeDragDrop(window); }

} // namespace cup::platform
