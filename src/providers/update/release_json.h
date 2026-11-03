#pragma once
// GitHub's "latest release" JSON, read without WinRT (decision 076): the
// file search service has no WinRT, and checks releases for itself. The
// same fields as the app's reader in github.cpp: tag_name, html_url, body
// (for the mandatory marker), and each asset's name, browser_download_url
// and digest. Pure, tested on Linux.

#include "providers/update/release.h"

#include <optional>
#include <string_view>

namespace cup {

// Nothing if it isn't JSON or isn't an object. Missing or non-string fields
// read as "", as in github.cpp: offerFor then finds nothing to offer.
std::optional<GitHubRelease> parseReleaseJson(std::string_view text);

} // namespace cup
