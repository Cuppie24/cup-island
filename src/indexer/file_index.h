#pragma once
// Every file name on one volume, by MFT record number (decision 075).
// An NTFS file reference is a record number (low 48 bits) and a sequence
// number (high 16, bumped when the record is reused). Record numbers are
// dense, so the index is a plain array addressed by them: 16 bytes a file
// plus its UTF-8 name, no hash table. ReFS's 128-bit references are refused
// for now (decision 075, later milestones).

#include "indexer/usn_record.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cup::indexer {

// The root folder's record on every NTFS volume; it is its own parent.
constexpr std::uint32_t kRootRecord = 5;
// Above this a reference is refused: 256 M records, far past any real MFT,
// and it keeps one bad record from asking for gigabytes.
constexpr std::uint64_t kMaxRecords = 1ull << 28;

// Entry::flags bits.
constexpr std::uint8_t kUsed = 1;
constexpr std::uint8_t kFolder = 2;
constexpr std::uint8_t kHidden = 4;
constexpr std::uint8_t kSystem = 8;
// A folder whose contents are rarely what anyone looks for (noiseFolderName):
// worked out from the name on every put(), never saved, so the list can
// change without a new snapshot format.
constexpr std::uint8_t kNoise = 16;

struct Entry {
    std::uint32_t parent = 0;     // the parent's record number
    std::uint32_t nameOffset = 0; // into the names buffer
    std::uint16_t nameLength = 0; // bytes of UTF-8
    std::uint16_t sequence = 0;   // the reference's high 16 bits
    std::uint8_t flags = 0;       // kUsed, kFolder, ...
};

// The record number a reference addresses, if the index can hold it.
std::optional<std::uint32_t> recordOf(FileRef ref);
// The reference for a record and its sequence number (NTFS layout).
FileRef refOf(std::uint32_t record, std::uint16_t sequence);
// Folders whose contents rank below everything else (query.h): Windows'
// component store (WinSxS) and Store apps' insides (WindowsApps), package and
// build caches (node_modules, __pycache__, .git, .vs), the recycle bin. Shown, never hidden. Matched
// whole, ignoring ASCII case. A fixed list: the index's logic, not a
// setting (decision 075).
bool noiseFolderName(std::string_view name);
// Entry flags for FILE_ATTRIBUTE_* bits (directory, hidden, system).
std::uint8_t flagsFromAttributes(std::uint32_t attributes);

class FileIndex {
public:
    // Adds the file or replaces what its record held (a rename, a move, or
    // a reused record). An unchanged name and parent appends nothing, so the
    // journal's many "data changed" records cost nothing. False if the
    // reference can't be held, it's the root folder, or the name is empty.
    bool put(FileRef file, FileRef parent, std::string_view name, std::uint8_t flags);
    // Removes the file if the record still holds that file (same sequence):
    // a delete for an older use of the record leaves the new one alone.
    bool remove(FileRef file);

    // The record holding `file`, if it's in the index.
    std::optional<std::uint32_t> find(FileRef file) const;
    bool used(std::uint32_t record) const;
    const Entry& entry(std::uint32_t record) const { return entries_[record]; }
    std::string_view name(std::uint32_t record) const;
    // `root` ("C:\") then each folder down to the file, '\'-separated. A
    // parent missing from the index ends the walk early: the path then
    // starts at the first folder that is known.
    std::string path(std::uint32_t record, std::string_view root) const;

    // Is any folder above this one (not the record itself) a noise folder?
    bool inNoise(std::uint32_t record) const;

    std::size_t count() const { return count_; }
    std::size_t slots() const { return entries_.size(); } // records 0..slots-1
    std::size_t memoryBytes() const;                      // what the index holds, roughly

    // Rewrites the names buffer without the bytes replaced names left
    // behind. put() calls it once they are more than half the buffer.
    void compact();
    // Gives back the room growing one file at a time leaves (up to half
    // again): after a full scan or a snapshot load, before live changes.
    void shrink();

private:
    std::vector<Entry> entries_;
    std::string names_;
    std::size_t count_ = 0;
    std::size_t wasted_ = 0; // bytes of names_ no entry points at
};

} // namespace cup::indexer
