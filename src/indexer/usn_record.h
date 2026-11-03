#pragma once
// USN records read from raw bytes (decision 075). The service hands over the
// buffers FSCTL_ENUM_USN_DATA and FSCTL_READ_USN_JOURNAL fill; parsing them
// here keeps every byte offset testable on Linux, with no Windows headers.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cup::indexer {

// A file reference: NTFS uses 64 bits (record number in the low 48, sequence
// in the high 16), ReFS 128. `high` is 0 for every NTFS reference.
struct FileRef {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    bool operator==(const FileRef&) const = default;
};

// What a record says about one file: USN_RECORD_V2 or V3, minus the fields
// the index never uses (time stamp, source info, security id).
struct UsnRecord {
    FileRef file;
    FileRef parent;
    std::int64_t usn = 0;
    std::uint32_t reason = 0;     // USN_REASON_* bits; 0 in an enumeration
    std::uint32_t attributes = 0; // FILE_ATTRIBUTE_* bits
    std::string name;             // UTF-8
};

// One buffer's worth. Both controls put 8 bytes first: the next file
// reference to enumerate from (ENUM_USN_DATA) or the next USN to read from
// (READ_USN_JOURNAL). `ok` is false when a record didn't fit or made no
// sense; the records before it are kept, the rest is dropped.
struct UsnBuffer {
    std::uint64_t next = 0;
    std::vector<UsnRecord> records;
    bool ok = true;
};

UsnBuffer parseUsnBuffer(std::span<const std::uint8_t> bytes);

// UTF-16LE (Windows' file names) to UTF-8. A lone surrogate, which NTFS
// allows in names, becomes U+FFFD. An odd trailing byte is ignored.
std::string utf16leToUtf8(std::span<const std::uint8_t> bytes);

} // namespace cup::indexer
