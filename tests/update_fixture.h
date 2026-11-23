#pragma once
// The updater's tests' fixture (decision 036): a GitHub release as the app
// would hand it over, and an UpdateProvider whose hooks record each fetch
// and install for the test to answer.

#include "panel_host.h"
#include "providers/update/update_provider.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cup::test {

using Done = std::function<void(std::optional<GitHubRelease>)>;
using Failed = std::function<void(std::string)>;

inline const std::string kSha = std::string(64, 'A'); // GitHub's digest, upper case to check it's lowered

inline GitHubRelease releaseOf(const std::string& version, std::string digest = "sha256:" + kSha)
{
    std::string name = "CupIsland-Setup-" + version + ".exe";
    return { .tag = "v" + version,
             .page = "https://github.com/Cuppie24/cup-island/releases/tag/v" + version,
             .assets = { { "CupIsland.pdb", "https://github.com/Cuppie24/cup-island/releases/download/v" + version + "/CupIsland.pdb", "sha256:" + std::string(64, 'b') },
                         { name, "https://github.com/Cuppie24/cup-island/releases/download/v" + version + "/" + name, std::move(digest) } } };
}

// A provider with hooks that record calls; the test answers them.
struct Updater {
    PanelHost host;
    std::vector<Done> fetches;
    std::vector<Failed> installs;
    std::vector<std::string> installed; // each install's version
    std::optional<UpdateStatus> status; // what Settings was last told
    UpdateProvider provider;

    explicit Updater(std::string version)
        : provider(UpdateHooks{ .version = std::move(version),
                                .fetchLatest = [this](Done d) { fetches.push_back(std::move(d)); },
                                .install = [this](const UpdateOffer& o, Failed f) {
                                    installed.push_back(versionText(o.version));
                                    installs.push_back(std::move(f));
                                } })
    {
        provider.onStatus([this](const std::optional<UpdateStatus>& s) { status = s; });
    }
    void wakeAndAnswer(std::optional<GitHubRelease> release)
    {
        host.clock = *host.wake;
        provider.onWake();
        fetches.back()(std::move(release));
    }
    // Update and restart: install() asks GitHub first (decision 172); this answers it.
    void installWith(std::optional<GitHubRelease> latest)
    {
        std::size_t asked = fetches.size();
        provider.install();
        if (fetches.size() > asked) fetches.back()(std::move(latest));
    }
};

} // namespace cup::test
