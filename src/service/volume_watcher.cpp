#include "service/volume_watcher.h"

#include "indexer/snapshot.h"
#include "service/service_limits.h"
#include "service/service_log.h"

#include <cwchar>
#include <fstream>
#include <iterator>
#include <mutex>

namespace cup::service {
namespace {

using indexer::VolumeState;

// Only records that can change a name, a folder or the flags we keep wake
// the watcher; writes to a file's contents don't.
constexpr DWORD kReasons = USN_REASON_FILE_CREATE | USN_REASON_FILE_DELETE | USN_REASON_RENAME_OLD_NAME |
                           USN_REASON_RENAME_NEW_NAME | USN_REASON_BASIC_INFO_CHANGE | USN_REASON_HARD_LINK_CHANGE;

bool journalLost(DWORD error)
{
    return error == ERROR_JOURNAL_ENTRY_DELETED || error == ERROR_JOURNAL_DELETE_IN_PROGRESS ||
           error == ERROR_JOURNAL_NOT_ACTIVE || error == ERROR_INVALID_PARAMETER; // the last: our journal id is stale
}

} // namespace

VolumeWatcher::VolumeWatcher(VolumeInfo volume, std::filesystem::path snapshotFolder, HANDLE stopEvent)
    : volume_(std::move(volume)), snapshotFolder_(std::move(snapshotFolder)), stop_(stopEvent)
{
}

VolumeWatcher::~VolumeWatcher()
{
    if (thread_.joinable()) thread_.join();
}

void VolumeWatcher::start()
{
    thread_ = std::thread([this] { run(); });
}

void VolumeWatcher::run()
{
    const char* root = volume_.root.c_str();
    bool tryLoad = true;
    int failures = 0;
    for (;;) {
        if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) break;
        Outcome outcome = Outcome::Failed;
        if (!handle_.valid()) handle_ = openVolume(volume_);
        indexer::JournalNow now;
        DWORD error = handle_.valid() ? queryJournal(handle_.get(), stop_, now) : GetLastError();
        if (error == ERROR_JOURNAL_NOT_ACTIVE) {
            serviceLog("%s: no change journal, creating one", root);
            error = createJournal(handle_.get(), stop_);
            if (!error) error = queryJournal(handle_.get(), stop_, now);
        }
        if (error == ERROR_OPERATION_ABORTED) break;
        if (error) {
            serviceLog("%s: can't open the drive or its journal (error %lu)", root, error);
        } else {
            bool indexed = tryLoad && loadSnapshot(now);
            tryLoad = false;
            outcome = indexed ? Outcome::Scanned : scan(now);
            if (outcome == Outcome::Scanned) outcome = follow();
        }
        if (outcome == Outcome::Stopped) break;
        if (outcome == Outcome::Rescan) {
            serviceLog("%s: lost track of the journal, scanning again", root);
            continue;
        }
        state_ = VolumeState::Failed;
        if (++failures > kMaxVolumeRetries) {
            serviceLog("%s: giving up after %d tries", root, failures);
            break;
        }
        handle_.reset();
        if (WaitForSingleObject(stop_, kVolumeRetryMs) == WAIT_OBJECT_0) break;
    }
    save();
}

bool VolumeWatcher::loadSnapshot(const indexer::JournalNow& now)
{
    std::ifstream in(snapshotFile(), std::ios::binary);
    if (!in) return false;
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto snapshot = indexer::loadSnapshot(bytes);
    if (!snapshot) {
        serviceLog("%s: saved index unreadable, scanning", volume_.root.c_str());
        return false;
    }
    if (indexer::resumeFrom(snapshot->mark, now) == indexer::Resume::Rescan) {
        serviceLog("%s: saved index is too old for the journal, scanning", volume_.root.c_str());
        return false;
    }
    std::size_t count = snapshot->index.count();
    {
        std::unique_lock guard(lock_);
        index_ = std::move(snapshot->index);
    }
    mark_ = snapshot->mark;
    savedMark_ = mark_;
    files_ = count;
    state_ = VolumeState::Ready;
    serviceLog("%s: loaded %zu files, catching up from the journal", volume_.root.c_str(), count);
    return true;
}

VolumeWatcher::Outcome VolumeWatcher::scan(const indexer::JournalNow& now)
{
    state_ = VolumeState::Scanning;
    ULONGLONG began = GetTickCount64();
    indexer::FileIndex fresh;
    // Everything up to the journal's position when the scan began; changes
    // during the scan are then replayed from that position (harmless twice).
    EnumRange range{ 0, 0, now.nextUsn };
    std::vector<std::uint8_t> buffer(kEnumBufferBytes);
    for (;;) {
        DWORD bytes = 0;
        DWORD error = ioctlNow(handle_.get(), stop_, FSCTL_ENUM_USN_DATA, &range, sizeof range, buffer.data(),
                               static_cast<DWORD>(buffer.size()), bytes);
        if (error == ERROR_HANDLE_EOF) break; // past the last record
        if (error == ERROR_OPERATION_ABORTED) return Outcome::Stopped;
        if (error) {
            serviceLog("%s: reading the file table failed (error %lu)", volume_.root.c_str(), error);
            return Outcome::Failed;
        }
        indexer::UsnBuffer parsed = indexer::parseUsnBuffer(std::span<const std::uint8_t>(buffer.data(), bytes));
        for (const indexer::UsnRecord& record : parsed.records) indexer::applyRecord(fresh, record);
        if (!parsed.ok || parsed.next <= range.startFileReference) {
            serviceLog("%s: the file table read back malformed", volume_.root.c_str());
            return Outcome::Failed;
        }
        range.startFileReference = parsed.next;
    }
    fresh.shrink();
    std::size_t count = fresh.count();
    {
        std::unique_lock guard(lock_);
        index_ = std::move(fresh);
    }
    mark_ = { now.journalId, now.nextUsn };
    files_ = count;
    state_ = VolumeState::Ready;
    serviceLog("%s: scanned %zu files in %llu ms", volume_.root.c_str(), count, GetTickCount64() - began);
    unsaved_ = 1;
    save();
    return Outcome::Scanned;
}

VolumeWatcher::Outcome VolumeWatcher::follow()
{
    JournalRead read{};
    read.reasonMask = kReasons;
    read.returnOnlyOnClose = FALSE;
    read.timeout = 0;
    read.bytesToWaitFor = 1; // wait in the kernel until there's a record: no polling (TR §8)
    read.journalId = mark_.journalId;
    std::vector<std::uint8_t> buffer(kJournalBufferBytes);
    AsyncIoctl call(handle_.get());
    for (;;) {
        read.startUsn = mark_.nextUsn;
        if (DWORD error = call.start(FSCTL_READ_USN_JOURNAL, &read, sizeof read, buffer.data(), static_cast<DWORD>(buffer.size())))
            return journalLost(error) ? Outcome::Rescan : Outcome::Failed;
        DWORD bytes = 0, error = 0;
        IoResult result;
        for (;;) {
            DWORD timeout = INFINITE;
            if (unsaved_) {
                ULONGLONG due = firstUnsavedAt_ + kSaveDelayMs, now = GetTickCount64();
                timeout = due > now ? static_cast<DWORD>(due - now) : 0;
            }
            result = call.wait(stop_, timeout, bytes, error);
            if (result != IoResult::Timeout) break;
            save(); // the read stays in flight
        }
        if (result == IoResult::Stopped) return Outcome::Stopped;
        if (result == IoResult::Failed) {
            if (!journalLost(error)) serviceLog("%s: reading the journal failed (error %lu)", volume_.root.c_str(), error);
            return journalLost(error) ? Outcome::Rescan : Outcome::Failed;
        }
        indexer::UsnBuffer parsed = indexer::parseUsnBuffer(std::span<const std::uint8_t>(buffer.data(), bytes));
        if (!parsed.ok) return Outcome::Rescan;
        std::size_t changes = 0;
        {
            std::unique_lock guard(lock_);
            for (const indexer::UsnRecord& record : parsed.records)
                if (indexer::applyRecord(index_, record) != indexer::Change::Ignored) ++changes;
            files_ = index_.count();
        }
        if (changes && !unsaved_) firstUnsavedAt_ = GetTickCount64();
        unsaved_ += changes;
        mark_.nextUsn = static_cast<std::int64_t>(parsed.next);
    }
}

void VolumeWatcher::save()
{
    if (!unsaved_ && mark_ == savedMark_) return;
    std::string bytes;
    {
        std::shared_lock guard(lock_);
        bytes = indexer::saveSnapshot(index_, mark_);
    }
    std::error_code ec;
    std::filesystem::create_directories(snapshotFolder_, ec);
    std::filesystem::path file = snapshotFile(), side = file;
    side += L".new";
    {
        std::ofstream out(side, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            serviceLog("%s: saving the index failed", volume_.root.c_str());
            return;
        }
    }
    // A crash mid-write leaves the old file, never half of each (as decision 028).
    if (!MoveFileExW(side.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        serviceLog("%s: saving the index failed (error %lu)", volume_.root.c_str(), GetLastError());
        return;
    }
    unsaved_ = 0;
    savedMark_ = mark_;
}

std::vector<Found> VolumeWatcher::find(const indexer::Query& query, std::size_t limit) const
{
    std::vector<Found> out;
    std::shared_lock guard(lock_);
    for (const indexer::Hit& hit : indexer::search(index_, query, limit)) {
        const indexer::Entry& e = index_.entry(hit.record);
        bool inFolder = index_.used(e.parent) && e.parent != hit.record;
        out.push_back({ hit, std::string(index_.name(hit.record)), index_.path(hit.record, volume_.root),
                        inFolder ? index_.path(e.parent, volume_.root) : volume_.root, e.parent,
                        (e.flags & indexer::kHidden) != 0 });
    }
    return out;
}

indexer::VolumeStatus VolumeWatcher::status() const
{
    return { volume_.root, state_.load(), files_.load() };
}

std::filesystem::path VolumeWatcher::snapshotFile() const
{
    wchar_t name[32];
    std::swprintf(name, 32, L"volume-%08lX.bin", static_cast<unsigned long>(volume_.serial));
    return snapshotFolder_ / name;
}

} // namespace cup::service
