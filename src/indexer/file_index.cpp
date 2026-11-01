#include "indexer/file_index.h"

#include <algorithm>
#include <limits>

namespace cup::indexer {
namespace {

constexpr std::uint64_t kRecordMask = (1ull << 48) - 1;
// Deep enough for any real tree (MAX_PATH-free paths still stop well short
// of this); a loop in corrupt data can't spin for ever.
constexpr int kMaxDepth = 1024;

// FILE_ATTRIBUTE_* (winnt.h)
constexpr std::uint32_t kAttributeHidden = 0x2;
constexpr std::uint32_t kAttributeSystem = 0x4;
constexpr std::uint32_t kAttributeDirectory = 0x10;

} // namespace

std::optional<std::uint32_t> recordOf(FileRef ref)
{
    std::uint64_t record = ref.low & kRecordMask;
    if (ref.high != 0 || record >= kMaxRecords) return std::nullopt;
    return static_cast<std::uint32_t>(record);
}

FileRef refOf(std::uint32_t record, std::uint16_t sequence)
{
    return { static_cast<std::uint64_t>(record) | (static_cast<std::uint64_t>(sequence) << 48), 0 };
}

std::uint8_t flagsFromAttributes(std::uint32_t attributes)
{
    std::uint8_t flags = 0;
    if (attributes & kAttributeDirectory) flags |= kFolder;
    if (attributes & kAttributeHidden) flags |= kHidden;
    if (attributes & kAttributeSystem) flags |= kSystem;
    return flags;
}

bool FileIndex::put(FileRef file, FileRef parent, std::string_view name, std::uint8_t flags)
{
    auto record = recordOf(file);
    auto parentRecord = recordOf(parent);
    // The root folder is the volume itself: paths start at `root`, not a name.
    if (!record || !parentRecord || *record == kRootRecord || name.empty() || name.size() > std::numeric_limits<std::uint16_t>::max()) return false;
    if (*record >= entries_.size()) entries_.resize(*record + 1);
    Entry& e = entries_[*record];
    auto sequence = static_cast<std::uint16_t>(file.low >> 48);
    bool wasUsed = e.flags & kUsed;
    bool sameName = wasUsed && this->name(*record) == name;
    e.parent = *parentRecord;
    e.sequence = sequence;
    e.flags = static_cast<std::uint8_t>((flags & ~kUsed) | kUsed);
    if (!wasUsed) ++count_;
    if (sameName) return true;
    if (wasUsed) wasted_ += e.nameLength;
    e.nameOffset = static_cast<std::uint32_t>(names_.size());
    e.nameLength = static_cast<std::uint16_t>(name.size());
    names_.append(name);
    if (wasted_ > names_.size() / 2) compact();
    return true;
}

bool FileIndex::remove(FileRef file)
{
    auto record = find(file);
    if (!record) return false;
    Entry& e = entries_[*record];
    wasted_ += e.nameLength;
    e = Entry{};
    --count_;
    if (wasted_ > names_.size() / 2) compact();
    return true;
}

std::optional<std::uint32_t> FileIndex::find(FileRef file) const
{
    auto record = recordOf(file);
    if (!record || !used(*record)) return std::nullopt;
    if (entries_[*record].sequence != static_cast<std::uint16_t>(file.low >> 48)) return std::nullopt;
    return record;
}

bool FileIndex::used(std::uint32_t record) const
{
    return record < entries_.size() && (entries_[record].flags & kUsed);
}

std::string_view FileIndex::name(std::uint32_t record) const
{
    if (!used(record)) return {};
    const Entry& e = entries_[record];
    return std::string_view(names_).substr(e.nameOffset, e.nameLength);
}

std::string FileIndex::path(std::uint32_t record, std::string_view root) const
{
    if (!used(record)) return {};
    std::vector<std::uint32_t> chain{ record };
    std::uint32_t at = record;
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        std::uint32_t parent = entries_[at].parent;
        if (parent == at || parent == kRootRecord || !used(parent)) break;
        chain.push_back(parent);
        at = parent;
    }
    std::string out(root);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (it != chain.rbegin()) out += '\\';
        out += name(*it);
    }
    return out;
}

std::size_t FileIndex::memoryBytes() const
{
    return entries_.capacity() * sizeof(Entry) + names_.capacity();
}

void FileIndex::compact()
{
    std::string packed;
    packed.reserve(names_.size() - wasted_);
    for (Entry& e : entries_) {
        if (!(e.flags & kUsed)) continue;
        std::uint32_t offset = static_cast<std::uint32_t>(packed.size());
        packed.append(names_, e.nameOffset, e.nameLength);
        e.nameOffset = offset;
    }
    names_ = std::move(packed);
    wasted_ = 0;
}

void FileIndex::shrink()
{
    compact();
    entries_.shrink_to_fit();
    names_.shrink_to_fit();
}

} // namespace cup::indexer
