// The updater (decision 036): versions, choosing what to offer, and the
// provider's checks, widget, alert and install, through a recording host.

#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/update/update_provider.h"

namespace cup::test {
namespace {

using Done = std::function<void(std::optional<Release>)>;
using Failed = std::function<void(std::string)>;

const std::string kSha = std::string(64, 'A'); // GitHub's digest, upper case to check it's lowered

Release releaseOf(const std::string& version, std::string digest = "sha256:" + kSha)
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
    std::vector<std::string> opened;
    UpdateProvider provider;

    explicit Updater(std::string version)
        : provider(UpdateHooks{ .version = std::move(version),
                                .fetchLatest = [this](Done d) { fetches.push_back(std::move(d)); },
                                .install = [this](const UpdateOffer&, Failed f) { installs.push_back(std::move(f)); },
                                .openPage = [this](const std::string& url) { opened.push_back(url); } })
    {
    }
    void act(const char* action) { provider.onAction({ "system.update", UpdateProvider::kPanelId, action, 0 }); }
    void wakeAndAnswer(std::optional<Release> release)
    {
        host.clock = *host.wake;
        provider.onWake();
        fetches.back()(std::move(release));
    }
};

void versionTests()
{
    expect(parseVersion("0.4.0") == Version{ 0, 4, 0 } && parseVersion("v12.0.3") == Version{ 12, 0, 3 }, "versions: X.Y.Z, with or without v");
    for (const char* dev : { "0.3.0-5-gabc1234", "0.3.0-dirty", "0.0.0-dev", "1.2", "1.2.3.4", "", "v", "1..2", "1234567.0.0", "a.b.c" })
        expect(!parseVersion(dev), std::string("versions: not a release: ") + dev);
    expect(Version{ 0, 10, 0 } > Version{ 0, 9, 9 } && Version{ 1, 0, 0 } > Version{ 0, 99, 99 }, "versions: compared as numbers");
    expect(installerName({ 0, 4, 0 }) == "CupIsland-Setup-0.4.0.exe", "versions: the installer's name");

    Version running{ 0, 3, 0 };
    auto offer = offerFor(running, releaseOf("0.4.0"));
    expect(offer && offer->version == Version{ 0, 4, 0 } && offer->url == "https://github.com/Cuppie24/cup-island/releases/download/v0.4.0/CupIsland-Setup-0.4.0.exe",
           "offer: a newer release's installer");
    expect(offer && offer->sha256 == std::string(64, 'a') && offer->page.find("/v0.4.0") != std::string::npos, "offer: its checksum, lowered, and page");
    expect(!offerFor(running, releaseOf("0.3.0")) && !offerFor(running, releaseOf("0.2.9")), "offer: never the same or older");
    expect(!offerFor(running, releaseOf("0.4.0", "")) && !offerFor(running, releaseOf("0.4.0", "sha256:123")) &&
           !offerFor(running, releaseOf("0.4.0", "md5:" + kSha)), "offer: no usable checksum, no offer");
    Release noInstaller = releaseOf("0.4.0");
    noInstaller.assets.pop_back();
    Release oddTag = releaseOf("0.4.0");
    oddTag.tag = "nightly";
    expect(!offerFor(running, noInstaller) && !offerFor(running, oddTag), "offer: no installer, or a tag that's no version");
    Release elsewhere = releaseOf("0.4.0");
    elsewhere.assets.back().url = "https://evil.example/CupIsland-Setup-0.4.0.exe";
    expect(!offerFor(running, elsewhere), "offer: an installer from anywhere but GitHub isn't");
    Release oddPage = releaseOf("0.4.0");
    oddPage.page = "file:///C:/Windows/System32/calc.exe";
    expect(offerFor(running, oddPage) && offerFor(running, oddPage)->page.empty(), "offer: a page off GitHub is dropped");
}

void providerTests()
{
    const Tokens& t = tokens();
    namespace A = update_action;

    {
        Updater dev("0.3.0-5-gabc1234");
        dev.provider.start(dev.host);
        expect(!dev.host.wake && dev.fetches.empty(), "a dev build never checks");
    }

    Updater u("0.3.0");
    u.host.clock = Ms{ 1000000 };
    u.provider.start(u.host);
    expect(u.host.wake == Ms{ 1000000 } + t.updateFirstCheckDelay && u.fetches.empty(), "the first check waits out startup");

    u.wakeAndAnswer(std::nullopt);
    expect(u.host.wake == u.host.clock + t.updateRetry && u.host.load(UpdateProvider::kLastCheckKey).empty(), "offline: try again sooner, nothing saved");

    u.wakeAndAnswer(releaseOf("0.3.0"));
    expect(u.host.wake == u.host.clock + t.updateCheckInterval && u.host.published.empty() && u.host.alerts.empty(),
           "up to date: nothing shown, next check in a day");
    expect(u.host.load(UpdateProvider::kLastCheckKey) == std::to_string(u.host.clock.count()), "the check's time is kept");

    u.wakeAndAnswer(releaseOf("0.4.0"));
    const Panel& offered = u.host.published.back();
    expect(offered.id == UpdateProvider::kPanelId && textAt(offered, 0, 0) == "Update" && textAt(offered, 1, 0) == "0.4.0",
           "newer: an Update widget with the version");
    expect(cardText(offered, 0) == "Cup Island 0.4.0" && cardText(offered, 1) == "You have 0.3.0" && cardText(offered, 2) == "Install and restart",
           "its card: what, from what, and Install");
    expect(u.host.alerts.size() == 1 && u.host.alerts[0].activityId == UpdateProvider::kPanelId, "an alert that opens the card");
    expect(u.host.load(UpdateProvider::kAnnouncedKey) == "0.4.0", "the alert is remembered");
    u.wakeAndAnswer(releaseOf("0.4.0"));
    expect(u.host.alerts.size() == 1, "the next day: no second alert for the same version");

    u.act(A::kWhatsNew);
    expect(u.opened.size() == 1 && u.opened[0].find("/v0.4.0") != std::string::npos, "What's new opens the release's page");

    u.act(A::kInstall);
    expect(u.installs.size() == 1 && cardText(u.host.published.back(), 1) == "Downloading and installing…", "Install: downloading");
    u.act(A::kInstall);
    expect(u.installs.size() == 1, "a second click while installing does nothing");
    u.wakeAndAnswer(releaseOf("0.5.0"));
    expect(cardText(u.host.published.back(), 1) == "Downloading and installing…", "a check mid-download leaves it be");
    u.installs[0]("Couldn't download the update");
    expect(cardText(u.host.published.back(), 1) == "Couldn't download the update" && cardText(u.host.published.back(), 2) == "Try again",
           "a failure says why, and offers to try again");
    u.act(A::kInstall);
    expect(u.installs.size() == 2, "Try again installs again");

    u.provider.stop();
    std::size_t shown = u.host.published.size();
    u.installs[1]("late");
    expect(u.host.published.size() == shown && !u.host.wake && u.host.removed.back() == UpdateProvider::kPanelId,
           "stopped: the widget goes, a late callback does nothing");

    // A restart soon after a check waits out the rest of the day.
    Updater again("0.3.0");
    again.host.store[UpdateProvider::kLastCheckKey] = "0";
    again.host.clock = Ms{ 3600000 };
    again.provider.start(again.host);
    expect(again.host.wake == Ms{ 3600000 } + t.updateCheckInterval - Ms{ 3600000 }, "restarted an hour after a check: the rest of the day");

    // Withdrawn: the release that was offered is gone again.
    Updater w("0.3.0");
    w.provider.start(w.host);
    w.wakeAndAnswer(releaseOf("0.4.0"));
    w.wakeAndAnswer(releaseOf("0.3.0"));
    expect(!w.host.removed.empty() && w.host.removed.back() == UpdateProvider::kPanelId, "a withdrawn release: the widget goes");
}

} // namespace

void runUpdateTests()
{
    versionTests();
    providerTests();
}

} // namespace cup::test
