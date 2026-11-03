#pragma once
// The index service's sizes and timings (decision 075). Not in tokens.json:
// that file is the island's, lives where the user can write it and is
// live-reloaded; a SYSTEM process must not take its settings from a file
// any user can change. These change with a new service build only.

#include <cstddef>
#include <cstdint>

namespace cup::service {

// The service's name for the Service Control Manager, and the pipe clients
// open. The island (milestone 5) and the installer (milestone 4) use these.
constexpr wchar_t kServiceName[] = L"CupIslandIndex";
constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\CupIslandIndex";

// FSCTL_ENUM_USN_DATA output per call: ~13 000 records of a 1.2 M-file
// drive at a time, so a scan is ~100 calls.
constexpr std::size_t kEnumBufferBytes = 1024 * 1024;
// FSCTL_READ_USN_JOURNAL output per call: plenty for a burst of changes.
constexpr std::size_t kJournalBufferBytes = 64 * 1024;
// A journal this service creates where none exists (Windows' own default
// for C: is similar). Only used when a drive has no journal at all.
constexpr std::uint64_t kJournalMaxBytes = 32ull * 1024 * 1024;
constexpr std::uint64_t kJournalDeltaBytes = 8ull * 1024 * 1024;

// The index is saved this long after the first unsaved change, and when
// the service stops. A crash loses at most this much, which the journal
// replays at the next start anyway.
constexpr std::uint32_t kSaveDelayMs = 10 * 60 * 1000;
// After a drive fails (unreadable, journal gone mid-read), it's tried
// again this much later, up to kMaxVolumeRetries times.
constexpr std::uint32_t kVolumeRetryMs = 30 * 1000;
constexpr int kMaxVolumeRetries = 5;

// Clients at once. The island holds one; cupindex one per run.
constexpr int kMaxClients = 16;
// A query asks each drive for this many times the hits wanted, since some
// are then hidden from the user (folder_access); at most kMaxCandidates.
constexpr std::size_t kCandidateFactor = 4;
constexpr std::size_t kMaxCandidates = 800;

} // namespace cup::service
