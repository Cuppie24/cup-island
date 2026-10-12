#pragma once
// Versions and GitHub releases, as the updater sees them (decision 036).
// Pure: parsing and choosing, tested on Linux. The app only fetches the
// release (github.h) and hands it over as a GitHubRelease.

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cup {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    auto operator<=>(const Version&) const = default;
};

// "0.4.0" or "v0.4.0". Anything else is nothing: a dev build's
// "0.3.0-5-gabc1234" or "0.0.0-dev" is not a release, so it never updates.
std::optional<Version> parseVersion(std::string_view text);
std::string versionText(const Version& v); // "0.4.0"

// The installer every release carries (the Release workflow uploads it).
std::string installerName(const Version& v); // "CupIsland-Setup-0.4.0.exe"

struct ReleaseAsset {
    std::string name;
    std::string url;    // its download link
    std::string digest; // as GitHub gives it: "sha256:<64 hex digits>"; may be empty
    bool operator==(const ReleaseAsset&) const = default;
};

// GitHub's latest release (never a draft or a pre-release).
struct GitHubRelease {
    std::string tag;  // "v0.4.0"
    std::string page; // its page on GitHub: what's new
    std::vector<ReleaseAsset> assets;
    bool operator==(const GitHubRelease&) const = default;
};

struct UpdateOffer {
    Version version;
    std::string url;      // the installer's download link
    std::string sha256;   // lowercase hex: the download must match it
    std::string fileName; // installerName(version)
    std::string page;
    bool operator==(const UpdateOffer&) const = default;
};

// What to offer: `latest` if it's newer than `running` and carries its
// installer with a SHA-256, downloaded from https://github.com/. Without a
// checksum there's nothing to check the download against, so nothing is
// offered; a page that isn't on GitHub is dropped (no "What's new").
std::optional<UpdateOffer> offerFor(const Version& running, const GitHubRelease& latest);

} // namespace cup
