#include "indexer/journal.h"

namespace cup::indexer {

Change applyRecord(FileIndex& index, const UsnRecord& record)
{
    if (record.reason & kReasonFileDelete) return index.remove(record.file) ? Change::Removed : Change::Ignored;
    if ((record.reason & kReasonRenameOldName) && !(record.reason & kReasonRenameNewName)) return Change::Ignored;
    bool known = index.find(record.file).has_value();
    if (!index.put(record.file, record.parent, record.name, flagsFromAttributes(record.attributes))) return Change::Ignored;
    return known ? Change::Updated : Change::Added;
}

Resume resumeFrom(JournalMark saved, JournalNow now)
{
    if (saved.journalId != now.journalId) return Resume::Rescan;
    if (saved.nextUsn < now.firstUsn || saved.nextUsn > now.nextUsn) return Resume::Rescan;
    return Resume::Replay;
}

} // namespace cup::indexer
