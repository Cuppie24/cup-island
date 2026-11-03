#pragma once
// One request in, its answer out (decision 075): status, or a search of
// every drive with the results the asking user may not see left out.

#include "indexer/folder_access.h"
#include "service/self_update.h"
#include "service/volume_watcher.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cup::service {

// Runs as the service; `access` checks folders as the client (pipe_server
// makes it so), the only part done with the client's rights.
std::string answer(std::string_view body, const std::vector<std::unique_ptr<VolumeWatcher>>& volumes,
                   indexer::FolderAccess& access, SelfUpdate& update);

// May the current thread's user list this folder? Opens it for listing; the
// open succeeds only if their rights allow it. Any failure means no, which
// is also what an identification-only or anonymous client gets. Call while
// impersonating the client.
bool canListFolder(const std::string& folderPath);

} // namespace cup::service
