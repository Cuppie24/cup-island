#pragma once
// Keeping the index current from the USN change journal, and deciding at
// start whether a saved index can catch up or the volume must be scanned
// again (decision 075).

#include "indexer/file_index.h"
#include "indexer/usn_record.h"

#include <cstdint>

namespace cup::indexer {

// USN_REASON_* bits the index acts on (winioctl.h).
constexpr std::uint32_t kReasonFileCreate = 0x00000100;
constexpr std::uint32_t kReasonFileDelete = 0x00000200;
constexpr std::uint32_t kReasonRenameOldName = 0x00001000;
constexpr std::uint32_t kReasonRenameNewName = 0x00002000;

enum class Change { Added, Updated, Removed, Ignored };

// Puts one record into the index: a delete removes the file (a file made and
// deleted between two reads arrives as both bits: removed), a rename's old
// name is skipped (its new name follows), anything else adds or updates the
// file. Enumeration records (reason 0) go through here too.
Change applyRecord(FileIndex& index, const UsnRecord& record);

// Where a saved index left the journal.
struct JournalMark {
    std::uint64_t journalId = 0;
    std::int64_t nextUsn = 0;
    bool operator==(const JournalMark&) const = default;
};

// The journal now (FSCTL_QUERY_USN_JOURNAL).
struct JournalNow {
    std::uint64_t journalId = 0;
    std::int64_t firstUsn = 0; // the oldest record still held
    std::int64_t nextUsn = 0;
};

enum class Resume { Replay, Rescan };

// Replay from the mark if the journal is the same one and still holds every
// record since it; otherwise (deleted and re-created, wrapped past the mark,
// or behind it, which a restored disk image can do) scan the volume again.
Resume resumeFrom(JournalMark saved, JournalNow now);

} // namespace cup::indexer
