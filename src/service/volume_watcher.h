#pragma once
// One drive's index, kept current on its own thread (decision 075):
// load the snapshot or scan the drive, then follow its journal until the
// service stops, saving now and then. Queries come from the pipe's threads,
// so the index sits behind a reader/writer lock: many searches at once,
// journal changes one batch at a time.

#include "indexer/file_index.h"
#include "indexer/journal.h"
#include "indexer/protocol.h"
#include "indexer/query.h"
#include "service/volume_io.h"

#include <atomic>
#include <filesystem>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace cup::service {

// A hit with what an answer needs, copied out under the lock.
struct Found {
    indexer::Hit hit;
    std::string name;
    std::string path;          // the whole path, from the drive's root
    std::string folderPath;    // the folder it's in ("C:\" for the root)
    std::uint32_t folder = 0;  // that folder's record, for folder_access
    bool hidden = false;
};

class VolumeWatcher {
public:
    VolumeWatcher(VolumeInfo volume, std::filesystem::path snapshotFolder, HANDLE stopEvent);
    ~VolumeWatcher(); // joins the thread; the stop event must be set first
    VolumeWatcher(const VolumeWatcher&) = delete;
    VolumeWatcher& operator=(const VolumeWatcher&) = delete;

    void start();
    std::vector<Found> find(const indexer::Query& query, std::size_t limit) const;
    indexer::VolumeStatus status() const;
    const VolumeInfo& volume() const { return volume_; }

private:
    enum class Outcome { Scanned, Stopped, Rescan, Failed };

    void run();
    bool loadSnapshot(const indexer::JournalNow& now); // true: the index is in and current
    Outcome scan(const indexer::JournalNow& now);
    Outcome follow();
    void save();
    std::filesystem::path snapshotFile() const;

    VolumeInfo volume_;
    std::filesystem::path snapshotFolder_;
    HANDLE stop_;
    Handle handle_;
    std::thread thread_;

    mutable std::shared_mutex lock_; // guards index_
    indexer::FileIndex index_;
    indexer::JournalMark mark_;      // only the watcher's thread touches it
    indexer::JournalMark savedMark_; // what the snapshot file holds
    std::atomic<indexer::VolumeState> state_{ indexer::VolumeState::Scanning };
    std::atomic<std::size_t> files_{ 0 };
    std::size_t unsaved_ = 0;        // changes since the last save
    ULONGLONG firstUnsavedAt_ = 0;   // GetTickCount64 of the first of them
};

} // namespace cup::service
