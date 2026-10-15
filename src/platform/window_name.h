#pragma once
// A window as a log line names it: "app.exe (class X, "title")". For finding
// out from a user's log which app did something to the island.

#include "platform/win.h"

#include <string>

namespace cup::platform {

std::wstring describeWindow(HWND hwnd);

} // namespace cup::platform
