#include "providers/update/release.h"

#include <algorithm>
#include <cctype>

namespace cup {
namespace {

// A run of digits at `at` (moved past it); nothing if there's none or it's absurdly long.
std::optional<int> number(std::string_view text, std::size_t& at)
{
    std::size_t start = at;
    int value = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9' && at - start < 6)
        value = value * 10 + (text[at++] - '0');
    if (at == start || (at < text.size() && text[at] >= '0' && text[at] <= '9')) return std::nullopt;
    return value;
}

// "sha256:ABC..." -> "abc...", if it's 64 hex digits.
std::optional<std::string> sha256Of(std::string_view digest)
{
    constexpr std::string_view prefix = "sha256:";
    if (digest.substr(0, prefix.size()) != prefix) return std::nullopt;
    std::string hex(digest.substr(prefix.size()));
    if (hex.size() != 64) return std::nullopt;
    for (char& c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return hex;
}

// Links from the release's JSON are only followed on GitHub itself: the
// download, and the page "What's new" opens in the browser.
bool onGitHub(std::string_view url) { return url.substr(0, 19) == "https://github.com/"; }

} // namespace

std::optional<Version> parseVersion(std::string_view text)
{
    std::size_t at = !text.empty() && text[0] == 'v' ? 1 : 0;
    Version v;
    auto major = number(text, at);
    if (!major || at >= text.size() || text[at++] != '.') return std::nullopt;
    auto minor = number(text, at);
    if (!minor || at >= text.size() || text[at++] != '.') return std::nullopt;
    auto patch = number(text, at);
    if (!patch || at != text.size()) return std::nullopt; // "-dev", "-5-gabc1234": not a release
    return Version{ *major, *minor, *patch };
}

std::string versionText(const Version& v)
{
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
}

std::string installerName(const Version& v) { return "CupIsland-Setup-" + versionText(v) + ".exe"; }

std::optional<UpdateOffer> offerFor(const Version& running, const GitHubRelease& latest)
{
    auto version = parseVersion(latest.tag);
    if (!version || *version <= running) return std::nullopt;
    std::string name = installerName(*version);
    auto asset = std::find_if(latest.assets.begin(), latest.assets.end(), [&](const ReleaseAsset& a) { return a.name == name; });
    if (asset == latest.assets.end() || !onGitHub(asset->url)) return std::nullopt;
    auto sha = sha256Of(asset->digest);
    if (!sha) return std::nullopt;
    return UpdateOffer{ .version = *version, .url = asset->url, .sha256 = *sha, .fileName = name,
                        .page = onGitHub(latest.page) ? latest.page : std::string() };
}

} // namespace cup
