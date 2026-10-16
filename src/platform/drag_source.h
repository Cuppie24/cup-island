#pragma once
// Files dragged out of the island (decision 065): Windows' own drag, with
// its drag image, as from Explorer. Copy or link only: never a move, so
// nothing leaves the shelf's store or the user's folder.

#include "platform/win.h"

#include <string>
#include <vector>

namespace cup::platform {

// Modal: returns when the drag ends (dropped or cancelled). `paths` in UTF-8.
void dragFilesOut(HWND window, const std::vector<std::string>& paths);

// A drag out is under way: the island's own drop target ignores it (decision 061).
bool draggingOut();

} // namespace cup::platform
