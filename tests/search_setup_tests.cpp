// File search's setup and updates (decision 076): when the app runs the
// bundled installer, when it asks the service to update, and when neither;
// plus the service's own update offer and GitHub's release JSON.

#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/search/search_setup.h"
#include "providers/update/release_json.h"

namespace cup::test {
namespace {

using Probed = std::function<void(ServiceProbe)>;
using Answered = std::function<void(std::optional<indexer::UpdateAnswer>)>;

// A provider whose hooks record calls; the test answers them.
struct Setup {
    PanelHost host;
    std::vector<Probed> probes;
    int installs = 0;
    bool installStarts = true;
    int updates = 0;
    SearchSetupProvider provider;

    explicit Setup(std::string version, bool bundled = true)
        : provider(SearchSetupHooks{ .appVersion = std::move(version),
                                     .installerBundled = bundled,
                                     .probe = [this](Probed p) { probes.push_back(std::move(p)); },
                                     .runInstaller = [this](std::function<void(bool)> d) { ++installs; d(installStarts); },
                                     .askUpdate = [this](Answered a) { ++updates; a(indexer::UpdateAnswer::Started); } })
    {
    }

    // Starts, waits for the wake-up, and answers its probe.
    void run(ServiceProbe probe)
    {
        provider.start(host);
        if (!host.wake) return;
        host.clock = *host.wake;
        provider.onWake();
        probes.back()(std::move(probe));
    }
};

void providerTests()
{
    using State = ServiceProbe::State;
    using Step = SearchSetupProvider::Step;
    {
        Setup s("0.8.0");
        s.provider.start(s.host);
        expect(s.host.wake == Ms{ tokens().searchSetupDelay } && s.probes.empty(), "it waits searchSetupDelay before looking");
    }
    {
        Setup s("0.8.0");
        s.run({ State::Missing, "" });
        expect(s.installs == 1 && s.provider.step() == Step::Installing, "missing: the bundled installer runs");
        expect(s.host.alerts.size() == 1 && s.host.alerts[0].content.label == "Setting up file search", "saying Windows will ask, first");
        expect(s.host.store[SearchSetupProvider::kAskedKey] == "0.8.0", "and remembering it asked for this version");
        expect(!s.host.wake, "once per start");
    }
    {
        Setup s("0.8.0");
        s.host.store[SearchSetupProvider::kAskedKey] = "0.8.0"; // refused last time
        s.run({ State::Missing, "" });
        expect(s.installs == 0 && s.host.alerts.empty() && s.provider.step() == Step::NotInstalled,
               "refused for this version: not asked again");
    }
    {
        Setup s("0.8.1");
        s.host.store[SearchSetupProvider::kAskedKey] = "0.8.0";
        s.run({ State::Missing, "" });
        expect(s.installs == 1, "after the next update, asked once more");
    }
    {
        Setup s("0.8.0");
        s.installStarts = false;
        s.run({ State::Missing, "" });
        expect(s.provider.step() == Step::NotInstalled && s.host.store[SearchSetupProvider::kAskedKey] == "0.8.0",
               "an installer that won't start counts as asked");
    }
    {
        Setup s("0.8.0");
        s.run({ State::Running, "0.7.9" });
        expect(s.updates == 1 && s.installs == 0 && s.provider.step() == Step::Updating, "an older service is asked to update");
        Setup newer("0.8.0");
        newer.run({ State::Running, "0.9.0" });
        expect(newer.updates == 1, "so is a newer one: the latest release is what every copy runs (decision 040)");
    }
    {
        Setup s("0.8.0");
        s.run({ State::Running, "0.8.0" });
        expect(s.updates == 0 && s.provider.step() == Step::UpToDate, "the same version: nothing");
        Setup dev("0.8.0");
        dev.run({ State::Running, "0.8.0-3-gabc1234" });
        expect(dev.updates == 0, "a dev build of the service, run by hand, is left alone");
        Setup stopped("0.8.0");
        stopped.run({ State::Stopped, "" });
        expect(stopped.updates == 0 && stopped.installs == 0, "installed but stopped: Windows restarts it");
    }
    {
        Setup dev("0.7.2-4-g45dbf60");
        dev.provider.start(dev.host);
        Setup unbundled("0.8.0", false);
        unbundled.provider.start(unbundled.host);
        expect(!dev.host.wake && !unbundled.host.wake, "a dev build, or no bundled installer: never looks");
    }
    {
        // Gone before the probe answers: the answer goes nowhere.
        PanelHost host;
        Probed late;
        int installs = 0;
        auto provider = std::make_unique<SearchSetupProvider>(SearchSetupHooks{
            .appVersion = "0.8.0",
            .installerBundled = true,
            .probe = [&](Probed p) { late = std::move(p); },
            .runInstaller = [&](std::function<void(bool)> d) { ++installs; d(true); },
            .askUpdate = [](Answered) {} });
        provider->start(host);
        host.clock = *host.wake;
        provider->onWake();
        provider.reset();
        late({ State::Missing, "" });
        expect(installs == 0, "a probe answering after the provider is gone does nothing");
    }
}

void releaseTests()
{
    std::string sha(64, 'C');
    std::string json = R"({"tag_name":"v0.8.0","html_url":"https://github.com/Cuppie24/cup-island/releases/tag/v0.8.0",
        "body":"notes <!-- cup-island: mandatory -->","author":{"login":"x"},"assets":[
        {"name":"CupIsland-Setup-0.8.0.exe","browser_download_url":"https://github.com/Cuppie24/cup-island/releases/download/v0.8.0/CupIsland-Setup-0.8.0.exe","digest":"sha256:)" +
                       std::string(64, 'a') + R"("},
        {"name":"CupIsland-Index-Setup-0.8.0.exe","browser_download_url":"https://github.com/Cuppie24/cup-island/releases/download/v0.8.0/CupIsland-Index-Setup-0.8.0.exe","digest":"sha256:)" +
                       sha + R"(","uploader":{"login":"y"}},
        {"name":"CupIsland.pdb","browser_download_url":"https://github.com/x","digest":null}]})";
    auto release = parseReleaseJson(json);
    expect(release && release->tag == "v0.8.0" && release->assets.size() == 3 && release->mandatory, "GitHub's release JSON, without WinRT");
    expect(release && release->assets[2].digest.empty(), "a null digest reads as none");
    auto index = release ? indexOfferFor({ 0, 7, 9 }, *release) : std::nullopt;
    expect(index && index->fileName == "CupIsland-Index-Setup-0.8.0.exe" && index->sha256 == std::string(64, 'c') && !index->mandatory,
           "the service's own installer, its checksum lowered, never mandatory");
    auto app = release ? offerFor({ 0, 7, 9 }, *release) : std::nullopt;
    expect(app && app->fileName == "CupIsland-Setup-0.8.0.exe", "the app's offer is unchanged");
    expect(!indexOfferFor({ 0, 8, 0 }, *release), "already the latest: nothing");
    expect(!parseReleaseJson("[]") && !parseReleaseJson("nope"), "not a release");
}

} // namespace

void runSearchSetupTests()
{
    providerTests();
    releaseTests();
}

} // namespace cup::test
