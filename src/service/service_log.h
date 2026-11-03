#pragma once
// The index service's log (decision 075, like decision 013's for the app):
// one timestamped line per event in <data folder>\index.log, and on the
// console when run with --console. Its own small file, not platform/log:
// that one lives under the user's LocalAppData and links the app's
// clipboard code; the service runs as SYSTEM, with no user and no window.

#include <filesystem>

namespace cup::service {

// The service's own folder: snapshots and the log. Under LocalSystem that's
// SYSTEM's LocalAppData, under C:\Windows\System32\config\systemprofile,
// then CupIsland\index-service: ordinary users can't write to or pre-create
// it, unlike ProgramData. Run with --console by an administrator,
// it's that administrator's LocalAppData: fine for trying it out.
std::filesystem::path dataFolder();

// Opens <folder>\index.log for appending; over 1 MB it's first moved to
// index.old.log. echo: also print each line (console mode).
void startServiceLog(const std::filesystem::path& folder, bool echo);

// printf-style, any thread, flushed at once.
void serviceLog(const char* format, ...);

} // namespace cup::service
