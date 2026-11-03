#include "service/volume_io.h"

#include "service/service_limits.h"

namespace cup::service {

std::vector<VolumeInfo> fixedNtfsVolumes()
{
    std::vector<VolumeInfo> out;
    DWORD drives = GetLogicalDrives();
    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        if (!(drives & (1u << (letter - L'A')))) continue;
        std::wstring root = std::wstring(1, letter) + L":\\";
        if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) continue;
        wchar_t fileSystem[MAX_PATH + 1] = {};
        DWORD serial = 0, maxName = 0, flags = 0;
        if (!GetVolumeInformationW(root.c_str(), nullptr, 0, &serial, &maxName, &flags, fileSystem, MAX_PATH + 1)) continue;
        if (std::wstring_view(fileSystem) != L"NTFS") continue;
        out.push_back({ L"\\\\.\\" + std::wstring(1, letter) + L":", narrow(root), serial });
    }
    return out;
}

Handle openVolume(const VolumeInfo& volume)
{
    return Handle(CreateFileW(volume.device.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
}

AsyncIoctl::AsyncIoctl(HANDLE file) : file_(file), event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}

AsyncIoctl::~AsyncIoctl()
{
    cancel();
}

DWORD AsyncIoctl::start(DWORD code, const void* in, DWORD inBytes, void* out, DWORD outBytes)
{
    if (!event_.valid()) return ERROR_NOT_ENOUGH_MEMORY;
    cancel();
    ov_ = OVERLAPPED{};
    ov_.hEvent = event_.get();
    ResetEvent(ov_.hEvent);
    // A call that completes at once still signals the event and fills the
    // OVERLAPPED, so wait() reads both cases the same way.
    if (!DeviceIoControl(file_, code, const_cast<void*>(in), inBytes, out, outBytes, nullptr, &ov_)) {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) return error;
    }
    pending_ = true;
    return 0;
}

IoResult AsyncIoctl::wait(HANDLE stopEvent, DWORD timeoutMs, DWORD& bytes, DWORD& error)
{
    bytes = 0;
    error = 0;
    if (!pending_) {
        error = ERROR_INVALID_STATE;
        return IoResult::Failed;
    }
    // The call's event first: when both are set, a finished result wins.
    HANDLE events[2] = { ov_.hEvent, stopEvent };
    DWORD which = WaitForMultipleObjects(stopEvent ? 2 : 1, events, FALSE, timeoutMs);
    if (which == WAIT_TIMEOUT) return IoResult::Timeout;
    if (which == WAIT_OBJECT_0 + 1) {
        cancel();
        return IoResult::Stopped;
    }
    if (which != WAIT_OBJECT_0) {
        error = GetLastError();
        cancel();
        return IoResult::Failed;
    }
    pending_ = false;
    if (!GetOverlappedResult(file_, &ov_, &bytes, FALSE)) {
        error = GetLastError();
        return IoResult::Failed;
    }
    return IoResult::Done;
}

void AsyncIoctl::cancel()
{
    if (!pending_) return;
    CancelIoEx(file_, &ov_);
    DWORD bytes = 0;
    GetOverlappedResult(file_, &ov_, &bytes, TRUE); // the kernel is done with the buffers once this returns
    pending_ = false;
}

DWORD ioctlNow(HANDLE file, HANDLE stopEvent, DWORD code, const void* in, DWORD inBytes, void* out, DWORD outBytes, DWORD& bytes)
{
    AsyncIoctl call(file);
    if (DWORD error = call.start(code, in, inBytes, out, outBytes)) return error;
    DWORD error = 0;
    switch (call.wait(stopEvent, INFINITE, bytes, error)) {
    case IoResult::Done: return 0;
    case IoResult::Stopped: return ERROR_OPERATION_ABORTED;
    default: return error ? error : ERROR_GEN_FAILURE;
    }
}

DWORD queryJournal(HANDLE volume, HANDLE stopEvent, indexer::JournalNow& now)
{
    JournalData data{};
    DWORD bytes = 0;
    DWORD error = ioctlNow(volume, stopEvent, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &data, sizeof data, bytes);
    if (error) return error;
    if (bytes < sizeof data) return ERROR_INVALID_DATA;
    now.journalId = data.journalId;
    now.firstUsn = data.firstUsn;
    now.nextUsn = data.nextUsn;
    return 0;
}

DWORD createJournal(HANDLE volume, HANDLE stopEvent)
{
    CREATE_USN_JOURNAL_DATA create{ kJournalMaxBytes, kJournalDeltaBytes };
    DWORD bytes = 0;
    return ioctlNow(volume, stopEvent, FSCTL_CREATE_USN_JOURNAL, &create, sizeof create, nullptr, 0, bytes);
}

} // namespace cup::service
