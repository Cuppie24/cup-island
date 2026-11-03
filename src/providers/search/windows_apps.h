#pragma once
// Search's Windows side (decision 077): reading Windows' Applications and
// Control Panel folders, starting what's picked, opening files, and the
// facts Settings pages depend on (the build, the UI language).

#include "providers/search/launchables.h"

#include <functional>
#include <string>
#include <vector>

namespace cup {

// Apps (shell:AppsFolder) and Control Panel items, on a thread of its own
// (the shell wants a single-threaded COM apartment, and a few hundred items
// take a moment); `done` on the calling (UI) thread. Each target is the
// item's shell parsing path, which launchItem opens.
void loadWindowsApps(std::function<void(std::vector<Launchable>)> done);

// Starts an app or Control Panel item through the shell (desktop and Store
// apps alike, a shortcut's arguments kept), or opens a Settings page's link.
bool launchItem(const Launchable& item);
// Opens a file or folder as Explorer would (its default app, or a window).
bool openPath(const std::string& path);

int windowsBuild();  // 17763 for Windows 10 1809; 22000 and up for 11
bool russianUi();    // Windows' display language is Russian

} // namespace cup
