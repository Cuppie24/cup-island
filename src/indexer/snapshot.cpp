#include "indexer/snapshot.h"

#include <cstdint>

namespace cup::indexer {
namespace {

constexpr std::string_view kMagic = "CUPIDX";
// Bump when the layout changes: an old file is then rescanned, not misread.
constexpr std::uint16_t kFormat = 1;
constexpr std::uint8_t kKnownFlags = kFolder | kHidden | kSystem;

std::uint64_t fnv1a(std::string_view bytes)
{
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (char c : bytes) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 0x100000001b3ull;
    }
    return hash;
}

void put(std::string& out, std::uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i) out += static_cast<char>((value >> (8 * i)) & 0xFF);
}

// Reads fixed-size little-endian values; once a read runs past the end,
// every later read fails too, so callers check `ok` once at the end.
struct Reader {
    std::string_view bytes;
    std::size_t at = 0;
    bool ok = true;

    std::uint64_t take(int count)
    {
        if (!ok || bytes.size() - at < static_cast<std::size_t>(count)) {
            ok = false;
            return 0;
        }
        std::uint64_t value = 0;
        for (int i = 0; i < count; ++i) value |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(bytes[at + i])) << (8 * i);
        at += count;
        return value;
    }

    std::string_view text(std::size_t count)
    {
        if (!ok || bytes.size() - at < count) {
            ok = false;
            return {};
        }
        std::string_view out = bytes.substr(at, count);
        at += count;
        return out;
    }
};

} // namespace

std::string saveSnapshot(const FileIndex& index, JournalMark mark)
{
    std::string out(kMagic);
    put(out, kFormat, 2);
    put(out, mark.journalId, 8);
    put(out, static_cast<std::uint64_t>(mark.nextUsn), 8);
    put(out, index.slots(), 4);
    put(out, index.count(), 4);
    for (std::uint32_t record = 0; record < index.slots(); ++record) {
        if (!index.used(record)) continue;
        const Entry& e = index.entry(record);
        put(out, record, 4);
        put(out, e.parent, 4);
        put(out, e.sequence, 2);
        put(out, e.flags & kKnownFlags, 1);
        put(out, e.nameLength, 2);
        out += index.name(record);
    }
    put(out, fnv1a(out), 8);
    return out;
}

std::optional<Snapshot> loadSnapshot(std::string_view bytes)
{
    if (bytes.size() < kMagic.size() + 8 || bytes.substr(0, kMagic.size()) != kMagic) return std::nullopt;
    std::string_view body = bytes.substr(0, bytes.size() - 8);
    Reader check{ bytes, bytes.size() - 8 };
    if (check.take(8) != fnv1a(body)) return std::nullopt;

    Reader r{ body, kMagic.size() };
    if (r.take(2) != kFormat) return std::nullopt;
    Snapshot s;
    s.mark.journalId = r.take(8);
    s.mark.nextUsn = static_cast<std::int64_t>(r.take(8));
    // Every record is below `slots`, so one bad record number can't ask for
    // gigabytes; `slots` itself is held to the index's own cap.
    std::uint64_t slots = r.take(4);
    std::uint64_t count = r.take(4);
    if (slots > kMaxRecords || count > slots) return std::nullopt;
    std::int64_t lastRecord = -1;
    for (std::uint64_t i = 0; i < count && r.ok; ++i) {
        auto record = static_cast<std::uint32_t>(r.take(4));
        auto parent = static_cast<std::uint32_t>(r.take(4));
        auto sequence = static_cast<std::uint16_t>(r.take(2));
        auto flags = static_cast<std::uint8_t>(r.take(1));
        std::string_view name = r.text(r.take(2));
        // Written in record order, each once: anything else isn't ours.
        if (!r.ok || record >= slots || static_cast<std::int64_t>(record) <= lastRecord || (flags & ~kKnownFlags))
            return std::nullopt;
        if (!s.index.put(refOf(record, sequence), refOf(parent, 0), name, flags)) return std::nullopt;
        lastRecord = record;
    }
    if (!r.ok || r.at != body.size()) return std::nullopt;
    s.index.shrink();
    return s;
}

} // namespace cup::indexer
