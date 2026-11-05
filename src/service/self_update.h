#pragma once
// The service updating itself (decisions 076, 081). A client's `update`
// request names no version: the service asks GitHub for this repository's
// latest release itself and installs that release's service installer,
// checked against the SHA-256 the release gives, or nothing. So no client
// can make it install a file or version of its choosing.
//   - the bytes come from the app's own copy (the request's `installer`,
//     a plain local path) or, failing that, the release's attached file;
//     either way they're copied into the service's own folder, which no user
//     can write to, and only the release's hash decides;
//   - reopened locked (read-only, no one may write or delete it), hashed
//     through that handle and started while it's held: the bytes checked
//     are the bytes that run;
//   - the installer's TMP and TEMP point there too, not C:\Windows\Temp;
//   - at most one every kUpdateSpacingMs.
// "There" is the service's install folder, not its data folder: the installer
// is 32-bit, and it sees paths under System32 (where SYSTEM's LocalAppData is)
// redirected to SysWOW64, so it found no temp folder and quit (decision 087).
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
    // folder: where the service is installed (installFolder()); installers go
    // in <folder>\updates, the installer's log is <folder>\index-update.log.
    SelfUpdate(std::filesystem::path folder, std::string runningVersion);
    ~SelfUpdate(); // waits for a download in progress

    // Checks GitHub now (on the caller's thread: a client is waiting for the
    // answer); if there's a newer or other release, downloads and installs it
    // on a thread of its own and answers Started.
    // `installer`: the app's copy (UpdateRequest::installer), or empty.
    indexer::UpdateAnswer request(const std::string& installer);

    // Installers left by an update that didn't finish: deleted at start.
    void removeLeftovers();

private:
    void install(std::string url, std::string sha256, std::string fileName, std::string installer);

    std::filesystem::path folder_;
    std::string version_;
    std::mutex lock_;
    bool busy_ = false;
    ULONGLONG lastAt_ = 0;
    std::thread worker_;
    // The last installer started: an update request after it logs how it
    // ended, since it can't be waited for (it stops this service).
    Handle installer_;
};

} // namespace cup::service
