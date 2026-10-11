#pragma once
// Selects a browser tab through UI Automation (the tree a screen reader
// reads), so clicking the art lands on the tab that's playing (decision 005).
// Windows only. Blocking cross-process calls: run off the UI thread, in an
// initialized COM apartment.

#include "platform/win.h"

#include <string>
#include <vector>

namespace cup {

// Among one app's `windows` (most recently used first), finds the tab
// playing `mediaTitle` (UTF-8; see tab_match) and selects it. Returns the
// window holding it, or nullptr if no tab matched.
HWND selectMediaTab(const std::vector<HWND>& windows, const std::string& mediaTitle);

} // namespace cup
