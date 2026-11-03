#pragma once
// Talking to NTFS volumes (decision 075): which drives to index, opening
// one, and the USN controls. Overlapped I/O throughout, so a stop request
// interrupts a journal read that's waiting for the next change.

#include "indexer/journal.h"
#include "service/service_win.h"

#include <string>
#include <vector>

namespace cup::service {

struct VolumeInfo {
    std::wstring device; // \\.\C:
    std::string root;    // C:\ (UTF-8, how paths in answers start)
    DWORD serial = 0;    // names the snapshot file
};

// Fixed drives with a letter and NTFS. Others (FAT, exFAT, ReFS, network,
// removable, no letter) wait for later milestones.
std::vector<VolumeInfo> fixedNtfsVolumes();

// GENERIC_READ on the volume itself: the step that needs administrator.
Handle openVolume(const VolumeInfo& volume);

// The V0 layouts of the USN controls' structs (winioctl.h: USN_JOURNAL_DATA_V0,
// MFT_ENUM_DATA_V0, READ_USN_JOURNAL_DATA_V0). Declared here because SDKs
// differ in which versions they name, and V0 is what Windows 10 1809 takes
// for NTFS (decision 015); its layout never changes.
struct JournalData {
    DWORDLONG journalId;
    LONGLONG firstUsn;
    LONGLONG nextUsn;
    LONGLONG lowestValidUsn;
    LONGLONG maxUsn;
    DWORDLONG maximumSize;
    DWORDLONG allocationDelta;
};
struct EnumRange {
    DWORDLONG startFileReference;
    LONGLONG lowUsn;
    LONGLONG highUsn;
};
struct JournalRead {
    LONGLONG startUsn;
    DWORD reasonMask;
    DWORD returnOnlyOnClose;
    DWORDLONG timeout;
    DWORDLONG bytesToWaitFor;
    DWORDLONG journalId;
};
static_assert(sizeof(JournalData) == 56 && sizeof(EnumRange) == 24 && sizeof(JournalRead) == 40, "the V0 layouts");

enum class IoResult { Done, Stopped, Timeout, Failed };

// One DeviceIoControl in flight on an overlapped handle. Buffers passed to
// start() must outlive the call (until wait() says Done, Stopped or Failed).
class AsyncIoctl {
public:
    explicit AsyncIoctl(HANDLE file);
    ~AsyncIoctl(); // cancels a call still in flight, and waits for it
    AsyncIoctl(const AsyncIoctl&) = delete;
    AsyncIoctl& operator=(const AsyncIoctl&) = delete;

    DWORD start(DWORD code, const void* in, DWORD inBytes, void* out, DWORD outBytes); // 0, or the error
    // Timeout leaves the call in flight: wait again later.
    IoResult wait(HANDLE stopEvent, DWORD timeoutMs, DWORD& bytes, DWORD& error);
    void cancel();

private:
    HANDLE file_;
    Handle event_;
    OVERLAPPED ov_{};
    bool pending_ = false;
};

// start() then wait() with no timeout. Returns 0, the error, or
// ERROR_OPERATION_ABORTED when stopEvent was set.
DWORD ioctlNow(HANDLE file, HANDLE stopEvent, DWORD code, const void* in, DWORD inBytes, void* out, DWORD outBytes, DWORD& bytes);

// FSCTL_QUERY_USN_JOURNAL. ERROR_JOURNAL_NOT_ACTIVE: the drive has none.
DWORD queryJournal(HANDLE volume, HANDLE stopEvent, indexer::JournalNow& now);
// FSCTL_CREATE_USN_JOURNAL, kJournalMaxBytes big.
DWORD createJournal(HANDLE volume, HANDLE stopEvent);

} // namespace cup::service
