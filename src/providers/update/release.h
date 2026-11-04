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

// Whether a copy running `running` should move to `offered`, GitHub's
// latest release (decision 040): whenever it's a different version, higher
// or lower. The latest release is the one every copy runs, so a release made
// by mistake is undone by releasing the right one, not by its number.
bool movesTo(const Version& running, const Version& offered);

// The installer every release carries (the Release workflow uploads it).
std::string installerName(const Version& v); // "CupIsland-Setup-0.4.0.exe"
// The file search service's installer, in the same release (decision 076).
std::string indexInstallerName(const Version& v); // "CupIsland-Index-Setup-0.4.0.exe"

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
    bool mandatory = false; // its notes carry kMandatoryMarker (decision 041)
    // The service installer's SHA-256 from its notes (kIndexHashMarker),
    // lowercase hex; empty if none. The installer itself rides in the app's
    // installer, not on the release page (decision 081).
    std::string indexSha256;
    bool operator==(const GitHubRelease&) const = default;
};

// A release whose notes contain this is mandatory (decision 041): a copy
// that finds it installs it by itself at its next start, no click. The
// Release workflow writes it (an HTML comment: not shown on the page).
inline constexpr const char* kMandatoryMarker = "<!-- cup-island: mandatory -->";
bool isMandatory(std::string_view notes);

// The release notes say which service installer is the real one (decision
// 081): "<!-- cup-island: index-sha256 <64 hex digits> -->", written by the
// Release workflow. Its hash, lowercase; nothing if it's missing or malformed.
inline constexpr const char* kIndexHashMarker = "<!-- cup-island: index-sha256 ";
std::optional<std::string> indexHashIn(std::string_view notes);

struct UpdateOffer {
    Version version;
    std::string url;      // the installer's download link
    std::string sha256;   // lowercase hex: the download must match it
    std::string fileName; // installerName(version)
    std::string page;
    bool mandatory = false; // installs by itself at the next start (decision 041)
    bool operator==(const UpdateOffer&) const = default;
};

// What a check found, for a "Check for updates" button (Settings).
enum class CheckOutcome { UpToDate, Available, Failed, DevBuild };

// What to offer: `latest` if it's another version than `running` (movesTo) and carries its
// installer with a SHA-256, downloaded from https://github.com/. Without a
// checksum there's nothing to check the download against, so nothing is
// offered; a page that isn't on GitHub is dropped (no "What's new").
std::optional<UpdateOffer> offerFor(const Version& running, const GitHubRelease& latest);
// The file search service updating itself (decisions 076, 081): the
// release's service installer, never mandatory. Its SHA-256 is the notes'
// (indexSha256), else the attached asset's digest; `url` is the attached
// asset's, empty when there's none (the service then has only the copy the
// app brought). Nothing without a hash: nothing to check a file against.
std::optional<UpdateOffer> indexOfferFor(const Version& running, const GitHubRelease& latest);

// A path the service may read the app's copy of its installer from
// (decision 081): a drive letter's absolute path ("C:\\Users\\..."). Never a
// network path (\\\\server\\share: the service would sign in to whoever
// runs it), a device or \\\\?\\ path, or a file's stream ("x.exe:s").
bool plainLocalPath(std::string_view path);

} // namespace cup
