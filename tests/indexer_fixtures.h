#pragma once
// Raw USN record bytes for the indexer tests, laid out as winioctl.h's
// USN_RECORD_V2 and USN_RECORD_V3, so the parser is checked against the real
// offsets without a Windows machine.

#include "indexer/usn_record.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace cup::test {

inline void appendU16(std::vector<std::uint8_t>& b, std::uint16_t v)
{
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}

inline void appendU32(std::vector<std::uint8_t>& b, std::uint32_t v)
{
    appendU16(b, static_cast<std::uint16_t>(v));
    appendU16(b, static_cast<std::uint16_t>(v >> 16));
}

inline void appendU64(std::vector<std::uint8_t>& b, std::uint64_t v)
{
    appendU32(b, static_cast<std::uint32_t>(v));
    appendU32(b, static_cast<std::uint32_t>(v >> 32));
}

// One record: the version's header, the name in UTF-16LE, padded to 8 bytes.
inline void appendRecord(std::vector<std::uint8_t>& b, int major, indexer::FileRef file, indexer::FileRef parent,
                         std::int64_t usn, std::uint32_t reason, std::uint32_t attributes, std::u16string_view name)
{
    std::size_t header = major == 2 ? 60 : 76;
    std::size_t nameBytes = name.size() * 2;
    std::size_t length = (header + nameBytes + 7) / 8 * 8;
    appendU32(b, static_cast<std::uint32_t>(length));
    appendU16(b, static_cast<std::uint16_t>(major));
    appendU16(b, 0);
    appendU64(b, file.low);
    if (major == 3) appendU64(b, file.high);
    appendU64(b, parent.low);
    if (major == 3) appendU64(b, parent.high);
    appendU64(b, static_cast<std::uint64_t>(usn));
    appendU64(b, 0); // TimeStamp
    appendU32(b, reason);
    appendU32(b, 0); // SourceInfo
    appendU32(b, 0); // SecurityId
    appendU32(b, attributes);
    appendU16(b, static_cast<std::uint16_t>(nameBytes));
    appendU16(b, static_cast<std::uint16_t>(header));
    for (char16_t c : name) appendU16(b, c);
    for (std::size_t i = header + nameBytes; i < length; ++i) b.push_back(0);
}

inline void appendRecordV2(std::vector<std::uint8_t>& b, indexer::FileRef file, indexer::FileRef parent, std::int64_t usn,
                           std::uint32_t reason, std::uint32_t attributes, std::u16string_view name)
{
    appendRecord(b, 2, file, parent, usn, reason, attributes, name);
}

inline void appendRecordV3(std::vector<std::uint8_t>& b, indexer::FileRef file, indexer::FileRef parent, std::int64_t usn,
                           std::uint32_t reason, std::uint32_t attributes, std::u16string_view name)
{
    appendRecord(b, 3, file, parent, usn, reason, attributes, name);
}

} // namespace cup::test
