#include "indexer/usn_record.h"

namespace cup::indexer {
namespace {

// Little-endian reads; callers have checked the bounds.
std::uint16_t u16(std::span<const std::uint8_t> b, std::size_t at)
{
    return static_cast<std::uint16_t>(b[at] | (b[at + 1] << 8));
}

std::uint32_t u32(std::span<const std::uint8_t> b, std::size_t at)
{
    return static_cast<std::uint32_t>(u16(b, at)) | (static_cast<std::uint32_t>(u16(b, at + 2)) << 16);
}

std::uint64_t u64(std::span<const std::uint8_t> b, std::size_t at)
{
    return static_cast<std::uint64_t>(u32(b, at)) | (static_cast<std::uint64_t>(u32(b, at + 4)) << 32);
}

void appendUtf8(std::string& out, std::uint32_t c)
{
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}

// The fixed part of each version, up to and including FileNameOffset.
// V2: 8-byte references, V3: 16-byte ones; the same fields otherwise
// (winioctl.h, USN_RECORD_V2 / USN_RECORD_V3).
constexpr std::size_t kV2Header = 60;
constexpr std::size_t kV3Header = 76;

// One record at `at`, `length` bytes long (already checked to fit).
// False if its name doesn't fit inside it.
bool parseRecord(std::span<const std::uint8_t> b, std::size_t at, std::size_t length, std::uint16_t major, UsnRecord& r)
{
    std::size_t p = at + 8; // past RecordLength, MajorVersion, MinorVersion
    if (major == 2) {
        r.file = { u64(b, p), 0 };
        r.parent = { u64(b, p + 8), 0 };
        p += 16;
    } else {
        r.file = { u64(b, p), u64(b, p + 8) };
        r.parent = { u64(b, p + 16), u64(b, p + 24) };
        p += 32;
    }
    r.usn = static_cast<std::int64_t>(u64(b, p));
    r.reason = u32(b, p + 16); // after Usn and TimeStamp
    r.attributes = u32(b, p + 28); // after Reason, SourceInfo, SecurityId
    std::size_t nameLength = u16(b, p + 32);
    std::size_t nameOffset = u16(b, p + 34);
    std::size_t header = major == 2 ? kV2Header : kV3Header;
    if (nameOffset < header || nameLength % 2 != 0 || nameOffset + nameLength > length) return false;
    r.name = utf16leToUtf8(b.subspan(at + nameOffset, nameLength));
    return true;
}

} // namespace

UsnBuffer parseUsnBuffer(std::span<const std::uint8_t> bytes)
{
    UsnBuffer out;
    if (bytes.size() < 8) {
        out.ok = false;
        return out;
    }
    out.next = u64(bytes, 0);
    std::size_t at = 8;
    while (at < bytes.size()) {
        if (bytes.size() - at < 8) {
            out.ok = false;
            break;
        }
        std::size_t length = u32(bytes, at);
        std::uint16_t major = u16(bytes, at + 4);
        // A record is 8-byte aligned and never shorter than its own header;
        // a zero length would loop for ever.
        if (length < 8 || length % 8 != 0 || length > bytes.size() - at) {
            out.ok = false;
            break;
        }
        if (major == 2 || major == 3) {
            std::size_t header = major == 2 ? kV2Header : kV3Header;
            UsnRecord r;
            if (length < header || !parseRecord(bytes, at, length, major, r)) {
                out.ok = false;
                break;
            }
            out.records.push_back(std::move(r));
        }
        // V4 (range tracking) carries no name: skipped, like unknown versions.
        at += length;
    }
    return out;
}

std::string utf16leToUtf8(std::span<const std::uint8_t> bytes)
{
    std::string out;
    out.reserve(bytes.size() / 2);
    std::size_t units = bytes.size() / 2;
    for (std::size_t i = 0; i < units; ++i) {
        std::uint32_t c = u16(bytes, i * 2);
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < units) {
            std::uint32_t low = u16(bytes, (i + 1) * 2);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                appendUtf8(out, 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00));
                ++i;
                continue;
            }
        }
        if (c >= 0xD800 && c <= 0xDFFF) c = 0xFFFD; // a lone surrogate
        appendUtf8(out, c);
    }
    return out;
}

} // namespace cup::indexer
