#pragma once
// ServiceHost's Windows side (decision 078): finding Cup Island's service,
// sending it requests, and starting its bundled installer. The pipe calls
// block, so they run off the UI thread and answer on it.

#include "providers/service_host.h"

#include <functional>
#include <string>

namespace cup {

void probeService(std::function<void(ServiceProbe)> done);
// One request over the pipe, to the installed service only (index_client
// checks its process id against the Service Control Manager's).
void sendToService(std::string request, std::function<void(ServiceReply)> done);

// CupIsland-Index-Setup.exe beside CupIsland.exe: the installer puts it
// there; a dev build has none.
bool serviceInstallerBundled();
// Starts it elevated ("runas": Windows asks the user), silent once allowed,
// off the UI thread (the call waits for the user's answer). `done` on the UI
// thread: false if it couldn't start or the prompt was refused.
void runServiceInstaller(std::function<void(bool started)> done);

} // namespace cup
