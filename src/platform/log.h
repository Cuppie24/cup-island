#pragma once
// The app's log: one line per event, timestamped, to
// %LOCALAPPDATA%\CupIsland\logs\cup-island.log and to the console (Debug
// builds have one). Kept on disk so a freeze or crash in Release, where
// there's no console, can still be reported: Home's "Copy logs" tile copies it.
// No Windows types here: the Windows-only provider files log too.

#include <filesystem>

namespace cup::platform {

// Opens the log for appending. Over 2 MB, it's first moved to
// cup-island.old.log (one generation kept). Call once, first thing.
void startLog();

// printf-style; no trailing newline needed. Any thread. Each line is
// flushed at once, so the last words before a hang or crash are on disk.
void logLine(const char* format, ...);

// %LOCALAPPDATA%\CupIsland\logs (also where hang and crash dumps go). Empty if unknown.
std::filesystem::path logFolder();

// Puts the log's last ~64 KB on the clipboard as text. owner: one of our
// windows (the clipboard needs one). False if anything failed.
bool copyLogToClipboard(void* ownerHwnd);

} // namespace cup::platform
