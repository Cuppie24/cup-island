#pragma once
// The Windows side of SearchSetupProvider (decision 076): finding the file
// search service, asking it to update, and starting its bundled installer.
// The pipe calls block, so they run off the UI thread and answer on it.

#include "indexer/protocol.h"
#include "providers/search/search_setup.h"

#include <functional>
#include <optional>

namespace cup {

void probeSearchService(std::function<void(ServiceProbe)> done);
void askSearchServiceUpdate(std::function<void(std::optional<indexer::UpdateAnswer>)> done);

// CupIsland-Index-Setup.exe beside CupIsland.exe: the installer puts it
// there; a dev build has none.
bool searchInstallerBundled();
// Starts it elevated ("runas": Windows asks the user), silent once allowed,
// off the UI thread (the call waits for the user's answer). `done` on the UI
// thread: false if it couldn't start or the prompt was refused.
void runSearchInstaller(std::function<void(bool started)> done);

} // namespace cup
