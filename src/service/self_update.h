#pragma once
// The service updating itself (decision 076). A client's `update` request
// names nothing: the service asks GitHub for this repository's latest
// release itself and installs that release's CupIsland-Index-Setup-<v>.exe,
// checked against GitHub's SHA-256, or nothing. So no client can make it
// install a file or version of its choosing.
//   - downloaded into the service's own folder, which no user can write to;
//   - reopened locked (read-only, no one may write or delete it), hashed
//     through that handle and started while it's held: the bytes checked
//     are the bytes that run;
//   - the installer's TMP and TEMP point there too, not C:\Windows\Temp;
//   - at most one every kUpdateSpacingMs.
// The installer stops this service (--stop), replaces it and starts the new one.

#include "indexer/protocol.h"
#include "service/service_win.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace cup::service {

class SelfUpdate {
public:
    // folder: the service's own (dataFolder()); downloads go in <folder>\updates.
    SelfUpdate(std::filesystem::path folder, std::string runningVersion);
    ~SelfUpdate(); // waits for a download in progress

    // Checks GitHub now (on the caller's thread: a client is waiting for the
    // answer); if there's a newer or other release, downloads and installs it
    // on a thread of its own and answers Started.
    indexer::UpdateAnswer request();

    // Installers left by an update that didn't finish: deleted at start.
    void removeLeftovers();

private:
    void install(std::string url, std::string sha256, std::string fileName);

    std::filesystem::path folder_;
    std::string version_;
    std::mutex lock_;
    bool busy_ = false;
    ULONGLONG lastAt_ = 0;
    std::thread worker_;
};

} // namespace cup::service
