#pragma once
// The index on disk between runs (decision 075): the used entries and where
// the journal was, so the next start replays changes instead of scanning.
// Bytes in, bytes out; the service writes them to a side file and renames it
// over the old one (as decision 028). Anything malformed is refused whole,
// and the caller scans the volume again.

#include "indexer/file_index.h"
#include "indexer/journal.h"

#include <optional>
#include <string>
#include <string_view>

namespace cup::indexer {

struct Snapshot {
    FileIndex index;
    JournalMark mark;
};

// Little-endian: "CUPIDX" + format version, the mark, the slot and entry counts, then
// per used entry its record, parent, sequence, flags and name; an FNV-1a
// checksum of everything before it closes the file, so a cut-off or
// damaged file is caught.
std::string saveSnapshot(const FileIndex& index, JournalMark mark);
std::optional<Snapshot> loadSnapshot(std::string_view bytes);

} // namespace cup::indexer
