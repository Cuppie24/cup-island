// The updater (decision 036): versions, choosing what to offer, and the
// provider's checks, widget, alert and install, through a recording host.

#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/update/update_provider.h"

namespace cup::test {
namespace {

using Done = std::function<void(std::optional<GitHubRelease>)>;
using Failed = std::function<void(std::string)>;

const std::string kSha = std::string(64, 'A'); // GitHub's digest, upper case to check it's lowered

GitHubRelease releaseOf(const std::string& version, std::string digest = "sha256:" + kSha)
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
    void wakeAndAnswer(std::optional<GitHubRelease> release)
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
    GitHubRelease noInstaller = releaseOf("0.4.0");
    noInstaller.assets.pop_back();
    GitHubRelease oddTag = releaseOf("0.4.0");
    oddTag.tag = "nightly";
    expect(!offerFor(running, noInstaller) && !offerFor(running, oddTag), "offer: no installer, or a tag that's no version");
    GitHubRelease elsewhere = releaseOf("0.4.0");
    elsewhere.assets.back().url = "https://evil.example/CupIsland-Setup-0.4.0.exe";
    expect(!offerFor(running, elsewhere), "offer: an installer from anywhere but GitHub isn't");
    GitHubRelease oddPage = releaseOf("0.4.0");
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
    expect(offered.id == UpdateProvider::kPanelId && textAt(offered, 0, 1) == "0.4.0" && textAt(offered, 1, 0) == "Update" &&
           offered.content.tileLeading.empty(), "newer: an Update widget with the version, stacked to fit a narrow column");
    expect(offered.content.badge && offered.content.badgeHome, "newer: a dot on the widget, and on Home's button (decision 038)");
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
    expect(!u.host.published.back().content.badge && !u.host.published.back().content.badgeHome, "Install: under way, no dots: nothing to do");
    u.act(A::kInstall);
    expect(u.installs.size() == 1, "a second click while installing does nothing");
    u.wakeAndAnswer(releaseOf("0.5.0"));
    expect(cardText(u.host.published.back(), 1) == "Downloading and installing…", "a check mid-download leaves it be");
    u.installs[0]("Couldn't download the update");
    expect(cardText(u.host.published.back(), 1) == "Couldn't download the update" && cardText(u.host.published.back(), 2) == "Try again",
           "a failure says why, and offers to try again");
    expect(u.host.published.back().content.badge && u.host.published.back().content.badgeHome, "a failure: the dots are back, it still needs doing");
    u.act(A::kInstall);
    expect(u.installs.size() == 2, "Try again installs again");

    u.provider.stop();
    std::size_t shown = u.host.published.size();
    u.installs[1]("late");
    expect(u.host.published.size() == shown && !u.host.wake && u.host.removed.back() == UpdateProvider::kPanelId,
           "stopped: the widget goes, a late callback does nothing");

    // The offer survives a restart (decision 039): back on Home at once, no
    // second alert; forgotten once installed, or when GitHub withdraws it.
    {
        Updater r("0.3.0");
        r.provider.start(r.host);
        r.wakeAndAnswer(releaseOf("0.4.0"));
        expect(!r.host.load(UpdateProvider::kOfferKey).empty() && r.host.alerts.size() == 1, "restart: the offer is saved");
        r.provider.stop();
        expect(!r.host.load(UpdateProvider::kOfferKey).empty(), "restart: closing the app doesn't forget it");
        std::size_t published = r.host.published.size();
        r.provider.start(r.host);
        expect(r.host.published.size() == published + 1 && r.host.published.back().id == UpdateProvider::kPanelId &&
               r.host.published.back().content.badge && r.host.alerts.size() == 1 && r.fetches.size() == 1,
               "restart: back on Home at once, with its dot, no second alert, no check yet");
        r.act(A::kInstall);
        expect(r.installs.size() == 1, "restart: and it installs");
        r.provider.stop();

        Updater updated("0.4.0"); // the same store after the update installed
        updated.host.store = r.host.store;
        updated.provider.start(updated.host);
        expect(updated.host.published.empty() && updated.host.load(UpdateProvider::kOfferKey).empty(),
               "restart: running that version now, the offer is forgotten");
        updated.provider.stop();

        Updater withdrawn("0.3.0");
        withdrawn.host.store = r.host.store;
        withdrawn.provider.start(withdrawn.host);
        withdrawn.wakeAndAnswer(releaseOf("0.3.0"));
        expect(withdrawn.host.removed.size() == 1 && withdrawn.host.load(UpdateProvider::kOfferKey).empty(),
               "restart: GitHub no longer offers it: withdrawn and forgotten");
        withdrawn.provider.stop();
    }

    // Settings follows the offer (decision 037): told when it comes, how the
    // install goes, and when it's withdrawn; its button installs the same way.
    {
        Updater w("0.3.0");
        std::vector<std::optional<UpdateStatus>> told;
        w.provider.onStatus([&](const std::optional<UpdateStatus>& s) { told.push_back(s); });
        expect(told.size() == 1 && !told[0], "status: nothing offered yet, said at once");
        w.provider.start(w.host);
        w.wakeAndAnswer(releaseOf("0.4.0"));
        expect(told.back() && told.back()->version == "0.4.0" && told.back()->stage == UpdateStage::Offered, "status: an offer from the daily check");
        w.provider.install();
        expect(w.installs.size() == 1 && told.back()->stage == UpdateStage::Installing, "status: install() installs, as the card's button");
        w.provider.install();
        expect(w.installs.size() == 1, "status: not twice");
        w.installs[0]("Couldn't download the update");
        expect(told.back()->stage == UpdateStage::Failed && told.back()->problem == "Couldn't download the update", "status: a failure, and why");
        w.wakeAndAnswer(releaseOf("0.3.0"));
        expect(!told.back(), "status: withdrawn, nothing offered");
        w.provider.install();
        expect(w.installs.size() == 1, "status: nothing to install");
        w.provider.stop();
    }

    // "Check for updates" (Settings): checks now and says what it found.
    {
        std::vector<std::pair<CheckOutcome, std::string>> said;
        auto note = [&](CheckOutcome o, const std::string& v) { said.push_back({ o, v }); };
        Updater dev("0.0.0-dev");
        dev.provider.start(dev.host);
        dev.provider.checkNow(note);
        expect(said.size() == 1 && said[0].first == CheckOutcome::DevBuild && dev.fetches.empty(), "check now: a dev build says so, asks nobody");

        Updater c("0.3.0");
        c.provider.start(c.host);
        said.clear();
        c.provider.checkNow(note);
        c.provider.checkNow(note);
        expect(c.fetches.size() == 1 && said.empty(), "check now: one request, however many clicks");
        c.fetches.back()(std::nullopt);
        expect(said.size() == 2 && said[0].first == CheckOutcome::Failed && said[1].first == CheckOutcome::Failed, "check now: offline, both told");
        said.clear();
        c.provider.checkNow(note);
        c.fetches.back()(releaseOf("0.3.0"));
        expect(said.size() == 1 && said[0].first == CheckOutcome::UpToDate && c.host.published.empty(), "check now: up to date");
        said.clear();
        c.provider.checkNow(note);
        c.fetches.back()(releaseOf("0.5.1"));
        expect(said.size() == 1 && said[0] == std::pair{ CheckOutcome::Available, std::string("0.5.1") } && !c.host.published.empty() &&
               c.host.alerts.size() == 1, "check now: found, offered on Home as any check would");
        expect(c.host.wake == c.host.clock + t.updateCheckInterval, "check now: the daily check counts from it");
        said.clear();
        c.provider.checkNow(note);
        c.provider.stop();
        c.fetches.back()(releaseOf("0.5.1"));
        expect(said.empty(), "check now: stopped before the answer, nobody is told");
    }

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
