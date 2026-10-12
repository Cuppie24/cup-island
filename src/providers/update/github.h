#pragma once
// The updater's Windows side (decision 036): reads the latest release from
// GitHub's API, downloads an installer, checks its SHA-256 and runs it.
// Windows-only, compiled into the app; every decision (when to check, what's
// newer, what to offer) is UpdateProvider's.

#include "providers/update/release.h"

#include <functional>
#include <optional>
#include <string>

namespace cup {

// GET https://api.github.com/repos/Cuppie24/cup-island/releases/latest.
// Call on the UI thread; `done` runs there too: the release, or nothing.
void fetchLatestRelease(const std::string& appVersion, std::function<void(std::optional<Release>)> done);

// Downloads the installer into %LOCALAPPDATA%\CupIsland\updates, checks its
// SHA-256 against the offer and starts it silently, then calls `quit` (the
// installer replaces the island and starts it again). Any failure calls
// `failed` with what to tell the user. Call on the UI thread; both run there.
void downloadAndInstall(const UpdateOffer& offer, const std::string& appVersion, std::function<void()> quit,
                        std::function<void(std::string)> failed);

// Deletes installers an earlier update left (the one that ran can't delete
// itself while it runs). Call once at startup.
void removeOldInstallers();

// Opens a page in the default browser.
void openInBrowser(const std::string& url);

} // namespace cup
