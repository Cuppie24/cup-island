#include "providers/shelf/shelf_files.h"

#include "platform/log.h"
#include "platform/text_input.h"
#include "platform/win.h"

#include <shellapi.h>

#include <atomic>
#include <fstream>
#include <memory>
#include <thread>

namespace cup::shelf_files {
namespace {

namespace fs = std::filesystem;
using PostToUi = std::function<void(std::function<void()>)>;

constexpr const wchar_t* kPartial = L".partial"; // a copy under way; renamed when it's whole

fs::path fromUtf8(const std::string& utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), n);
    return fs::path(wide);
}

std::string problemFor(DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return "The file is gone";
    case ERROR_ACCESS_DENIED: return "Access denied";
    case ERROR_DISK_FULL: return "The disk is full";
    case ERROR_REQUEST_ABORTED: return "Too large to copy";
    case ERROR_BAD_NETPATH:
    case ERROR_NETNAME_DELETED:
    case ERROR_UNEXP_NET_ERR: return "The network went away";
    default: return "Couldn't copy it (error " + std::to_string(error) + ")";
    }
}

// What CopyFile2's progress callback needs: the limit, and a way to say how far.
struct Copying {
    std::uint64_t maxBytes = 0;
    std::function<void(float)> report;
    float lastReported = 0;
};

COPYFILE2_MESSAGE_ACTION CALLBACK onCopyProgress(const COPYFILE2_MESSAGE* message, PVOID context)
{
    auto* copying = static_cast<Copying*>(context);
    if (message->Type != COPYFILE2_CALLBACK_CHUNK_FINISHED) return COPYFILE2_PROGRESS_CONTINUE;
    const auto& chunk = message->Info.ChunkFinished;
    std::uint64_t total = chunk.uliTotalFileSize.QuadPart;
    // Its size wasn't known when it was dropped (decision 061): the limit holds here.
    if (copying->maxBytes && total > copying->maxBytes) return COPYFILE2_PROGRESS_CANCEL;
    float fraction = total ? static_cast<float>(chunk.uliTotalBytesTransferred.QuadPart) / static_cast<float>(total) : 1.0f;
    if (fraction - copying->lastReported >= 0.05f) { // a republish every 5%, not every chunk
        copying->lastReported = fraction;
        copying->report(fraction);
    }
    return COPYFILE2_PROGRESS_CONTINUE;
}

// Runs on a worker thread. Returns the problem, or "" with the copy whole at `target`.
std::string copyFile(const fs::path& source, const fs::path& target, Copying& copying, std::uint64_t& bytes)
{
    fs::path partial = target;
    partial += kPartial;
    COPYFILE2_EXTENDED_PARAMETERS params{ sizeof params };
    params.dwCopyFlags = COPY_FILE_FAIL_IF_EXISTS;
    params.pProgressRoutine = onCopyProgress;
    params.pvCallbackContext = &copying;
    // CopyFile2 copies alternate data streams too: the Mark of the Web comes along (decision 061).
    HRESULT hr = CopyFile2(source.c_str(), partial.c_str(), &params);
    if (FAILED(hr)) {
        DWORD error = HRESULT_FACILITY(hr) == FACILITY_WIN32 ? HRESULT_CODE(hr) : static_cast<DWORD>(hr);
        DeleteFileW(partial.c_str());
        return problemFor(error);
    }
    if (!MoveFileExW(partial.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DWORD error = GetLastError();
        DeleteFileW(partial.c_str());
        return problemFor(error);
    }
    std::error_code ec;
    bytes = fs::file_size(target, ec);
    return "";
}

std::string writeFile(const fs::path& target, const std::string& contents, std::uint64_t& bytes)
{
    fs::path partial = target;
    partial += kPartial;
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!out) return "Couldn't save it";
    }
    std::error_code ec;
    fs::rename(partial, target, ec);
    if (ec) return "Couldn't save it";
    bytes = contents.size();
    return "";
}

// Only ever inside the store: a copy, never anything the user owns.
bool inStore(const fs::path& path)
{
    fs::path folder = path.parent_path();
    return !path.empty() && folder.parent_path() == storeFolder();
}

} // namespace

fs::path storeFolder() { return platform::logFolder().parent_path() / L"shelf"; }

void clearUnfinished()
{
    std::error_code ec;
    fs::create_directories(storeFolder(), ec);
    std::vector<fs::path> partial;
    for (auto it = fs::recursive_directory_iterator(storeFolder(), ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        if (it->path().extension() == kPartial) partial.push_back(it->path());
    for (const fs::path& p : partial) {
        fs::remove(p, ec);
        fs::remove(p.parent_path(), ec); // its item's folder, if that left it empty
    }
    if (!partial.empty()) platform::logLine("shelf: removed %zu unfinished copies", partial.size());
}

ShelfHooks hooks(PostToUi postToUi)
{
    ShelfHooks h;
    h.copy = [postToUi](const CopyJob& job, ShelfHooks::Progress progress, ShelfHooks::Done done) {
        std::thread([postToUi, job, progress, done] {
            fs::path folder = storeFolder() / std::to_wstring(job.id);
            std::error_code ec;
            fs::create_directories(folder, ec);
            fs::path target = folder / fromUtf8(job.name);
            std::uint64_t bytes = 0;
            std::string problem;
            if (job.source.empty()) {
                problem = writeFile(target, job.contents, bytes);
            } else {
                Copying copying{ job.maxBytes, [postToUi, progress](float f) { postToUi([progress, f] { progress(f); }); } };
                problem = copyFile(fromUtf8(job.source), target, copying, bytes);
            }
            if (!problem.empty()) fs::remove(folder, ec); // empty again
            std::string stored = problem.empty() ? platform::toUtf8(target.wstring()) : std::string();
            if (!problem.empty()) platform::logLine("shelf: copy %llu failed: %s", static_cast<unsigned long long>(job.id), problem.c_str());
            postToUi([done, stored, bytes, problem] { done(stored, bytes, problem); });
        }).detach();
    };
    h.removeCopy = [](const std::string& stored) {
        fs::path path = fromUtf8(stored);
        if (!inStore(path)) {
            platform::logLine("shelf: not removing a file outside its store");
            return;
        }
        std::error_code ec;
        fs::remove_all(path.parent_path(), ec); // the item's folder: just this copy
    };
    h.open = [](const std::string& path) {
        HINSTANCE result = ShellExecuteW(nullptr, nullptr, fromUtf8(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) platform::logLine("shelf: couldn't open an item (%lld)", static_cast<long long>(reinterpret_cast<INT_PTR>(result)));
    };
    h.checkLinks = [postToUi](const std::vector<std::string>& paths, std::function<void(std::vector<bool>)> done) {
        std::thread([postToUi, paths, done] {
            std::vector<bool> present;
            for (const std::string& p : paths) present.push_back(GetFileAttributesW(fromUtf8(p).c_str()) != INVALID_FILE_ATTRIBUTES);
            postToUi([done, present] { done(present); });
        }).detach();
    };
    return h;
}

} // namespace cup::shelf_files
