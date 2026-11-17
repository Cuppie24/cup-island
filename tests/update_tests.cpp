// The updater (decision 036): versions, choosing what to offer, and the
// provider's checks, the status Settings shows (decision 152: no widget, card
// or alert of its own) and install, through a recording host.

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
    std::optional<UpdateStatus> status; // what Settings was last told
    UpdateProvider provider;

    explicit Updater(std::string version)
        : provider(UpdateHooks{ .version = std::move(version),
                                .fetchLatest = [this](Done d) { fetches.push_back(std::move(d)); },
                                .install = [this](const UpdateOffer&, Failed f) { installs.push_back(std::move(f)); } })
    {
        provider.onStatus([this](const std::optional<UpdateStatus>& s) { status = s; });
    }
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
    expect(!offerFor(running, releaseOf("0.3.0")) && offerFor(running, releaseOf("0.2.9")),
           "offer: never the same; an older latest is offered too (decision 040)");
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
    expect(u.status && u.status->version == "0.4.0" && u.status->stage == UpdateStage::Offered && u.status->page.find("/v0.4.0") != std::string::npos &&
               u.status->download.rfind("https://github.com/Cuppie24/cup-island/releases/download/", 0) == 0 && !u.status->mandatory,
           "newer: offered to Settings, with its page (What's new) and installer");
    expect(u.host.published.empty() && u.host.alerts.empty(), "newer: no widget, card or alert of its own: Settings' gear gets the dot (decision 152)");
    u.wakeAndAnswer(releaseOf("0.4.0"));
    expect(u.host.alerts.empty() && u.status->version == "0.4.0", "the next day: still offered, still quiet");

    u.provider.install();
    expect(u.installs.size() == 1 && u.status->stage == UpdateStage::Installing, "Install: downloading");
    u.provider.install();
    expect(u.installs.size() == 1, "a second click while installing does nothing");
    u.wakeAndAnswer(releaseOf("0.5.0"));
    expect(u.status->stage == UpdateStage::Installing && u.status->version == "0.4.0", "a check mid-download leaves it be");
    u.installs[0]("Couldn't download the update");
    expect(u.status->stage == UpdateStage::Failed && u.status->problem == "Couldn't download the update", "a failure says why");
    u.provider.install();
    expect(u.installs.size() == 2, "Try again installs again");

    u.provider.stop();
    u.installs[1]("late");
    expect(!u.host.wake && !u.status, "stopped: Settings is told there's nothing; a late callback does nothing");

    // The offer survives a restart (decision 039): back in Settings at once;
    // forgotten once installed, or when GitHub withdraws it.
    {
        Updater r("0.3.0");
        r.provider.start(r.host);
        r.wakeAndAnswer(releaseOf("0.4.0"));
        expect(!r.host.load(UpdateProvider::kOfferKey).empty(), "restart: the offer is saved");
        r.provider.stop();
        expect(!r.host.load(UpdateProvider::kOfferKey).empty(), "restart: closing the app doesn't forget it");
        r.provider.start(r.host);
        expect(r.status && r.status->version == "0.4.0" && r.fetches.size() == 1, "restart: offered again at once, no check yet");
        r.provider.install();
        expect(r.installs.size() == 1, "restart: and it installs");
        r.provider.stop();

        Updater updated("0.4.0"); // the same store after the update installed
        updated.host.store = r.host.store;
        updated.provider.start(updated.host);
        expect(!updated.status && updated.host.load(UpdateProvider::kOfferKey).empty(), "restart: running that version now, the offer is forgotten");
        updated.provider.stop();

        Updater withdrawn("0.3.0");
        withdrawn.host.store = r.host.store;
        withdrawn.provider.start(withdrawn.host);
        withdrawn.wakeAndAnswer(releaseOf("0.3.0"));
        expect(!withdrawn.status && withdrawn.host.load(UpdateProvider::kOfferKey).empty(),
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
        expect(w.installs.size() == 1 && told.back()->stage == UpdateStage::Installing, "status: install() installs: Settings' button");
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
        expect(said.size() == 1 && said[0].first == CheckOutcome::UpToDate && !c.status, "check now: up to date");
        said.clear();
        c.provider.checkNow(note);
        c.fetches.back()(releaseOf("0.5.1"));
        expect(said.size() == 1 && said[0] == std::pair{ CheckOutcome::Available, std::string("0.5.1") } && c.status && c.status->version == "0.5.1",
               "check now: found, offered as any check would");
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
    expect(!w.status, "a withdrawn release: no longer offered");
}

} // namespace

// The latest release is the one to run (decision 040): a copy moves to it
// whenever it's another version, lower included, so a release made by
// mistake (5.x) is undone by releasing the right one (0.6.0) after it.
void latestReleaseTests()
{
    expect(movesTo({ 5, 3, 5 }, { 5, 3, 6 }) && movesTo({ 0, 5, 1 }, { 0, 6, 0 }), "latest: a higher one, as always");
    expect(movesTo({ 5, 3, 6 }, { 0, 6, 0 }) && offerFor({ 5, 3, 6 }, releaseOf("0.6.0")),
           "latest: a lower one too: 5.x (released by mistake) comes back to 0.6.0");
    expect(movesTo({ 0, 6, 0 }, { 0, 5, 1 }), "latest: a release pulled, so the latest is older again: back to it");
    expect(!movesTo({ 0, 6, 0 }, { 0, 6, 0 }) && !offerFor({ 0, 6, 0 }, releaseOf("0.6.0")), "latest: the same one: nothing");

    // A lower offer saved before a restart is still there after it.
    Updater back("5.3.6");
    back.provider.start(back.host);
    back.wakeAndAnswer(releaseOf("0.6.0"));
    back.provider.stop();
    back.provider.start(back.host);
    expect(back.status && back.status->version == "0.6.0", "latest: a lower offer survives a restart too");
    back.provider.stop();
}

// Mandatory releases (decision 041): found by a check, installed by itself
// at the next start, with an alert; at most kMandatoryTries times a version.
void mandatoryTests()
{
    expect(isMandatory(std::string("Fixes a crash.\n") + kMandatoryMarker) && !isMandatory("Fixes a crash."), "mandatory: the marker in the notes");
    GitHubRelease required = releaseOf("0.6.0");
    required.mandatory = true;
    expect(offerFor({ 0, 5, 3 }, required)->mandatory && !offerFor({ 0, 5, 3 }, releaseOf("0.6.0"))->mandatory, "mandatory: carried into the offer");

    Updater m("0.5.3");
    m.provider.start(m.host);
    m.wakeAndAnswer(required);
    expect(m.installs.empty() && m.status && m.status->mandatory, "mandatory: found mid-run, nothing installs yet; Settings says when it will");
    m.provider.stop();
    std::size_t alerts = m.host.alerts.size();
    m.provider.start(m.host);
    expect(m.installs.size() == 1 && m.host.alerts.size() == alerts + 1 && m.host.alerts.back().content.label == "Updating",
           "mandatory: the next start installs it, no click, and says so");
    m.installs.back()("Couldn't download the update");
    for (int start = 2; start <= UpdateProvider::kMandatoryTries + 1; ++start) {
        m.provider.stop();
        m.provider.start(m.host);
    }
    expect(m.installs.size() == static_cast<std::size_t>(UpdateProvider::kMandatoryTries),
           "mandatory: one try a start, kMandatoryTries in all; then it waits for a click");
    m.provider.install();
    expect(m.installs.size() == static_cast<std::size_t>(UpdateProvider::kMandatoryTries) + 1, "mandatory: the button still works");
    m.provider.stop();

    Updater installed("0.6.0"); // the update went in: the same store, now running it
    installed.host.store = m.host.store;
    installed.provider.start(installed.host);
    expect(installed.installs.empty() && installed.host.load(UpdateProvider::kOfferKey).empty() &&
           installed.host.load(UpdateProvider::kMandatoryTriesKey).empty(), "mandatory: installed, the offer and its tries are forgotten");
    installed.provider.stop();

    Updater optional("0.5.3");
    optional.provider.start(optional.host);
    optional.wakeAndAnswer(releaseOf("0.6.0"));
    optional.provider.stop();
    optional.provider.start(optional.host);
    expect(optional.installs.empty(), "mandatory: an ordinary release never installs by itself");
    optional.provider.stop();
}

void runUpdateTests()
{
    mandatoryTests();
    versionTests();
    providerTests();
    latestReleaseTests();
}

} // namespace cup::test
