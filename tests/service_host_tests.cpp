// Cup Island's service, from the host's side (decisions 076, 078): when it
// runs the bundled installer, when it asks the service to update, and when
// neither; providers' calls, checked against their grants; plus the
// service's own update offer and GitHub's release JSON.

#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/provider_registry.h"
#include "providers/service_host.h"
#include "providers/update/release_json.h"

namespace cup::test {
namespace {

using Probed = std::function<void(ServiceProbe)>;
using Replied = std::function<void(ServiceReply)>;

// A provider whose hooks record calls; the test answers them.
struct Setup {
    PanelHost host;
    std::vector<Probed> probes;
    int installs = 0;
    bool installStarts = true;
    int updates = 0;
    std::vector<std::string> sent; // requests, as they'd go over the pipe
    std::vector<Replied> replies;
    std::vector<std::string> opened; // pages opened in the browser
    ServiceHost provider;

    explicit Setup(std::string version, bool bundled = true)
        : provider(ServiceHostHooks{ .appVersion = std::move(version),
                                     .installerBundled = bundled,
                                     .installerPath = "C:\\Program\\CupIsland-Index-Setup.exe",
                                     .probe = [this](Probed p) { probes.push_back(std::move(p)); },
                                     .runInstaller = [this](std::function<void(bool)> d) { ++installs; d(installStarts); },
                                     .send = [this](std::string request, Replied r) {
                                         if (request.find("\"service.update\"") != std::string::npos) ++updates;
                                         sent.push_back(std::move(request));
                                         replies.push_back(std::move(r));
                                     },
                                     .openPage = [this](const std::string& url) { opened.push_back(url); } })
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
    using Step = ServiceHost::Step;
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
        expect(s.host.store[ServiceHost::kAskedKey] == "0.8.0", "and remembering it asked for this version");
        expect(!s.host.wake, "once per start");
    }
    {
        Setup s("0.8.0");
        s.host.store[ServiceHost::kAskedKey] = "0.8.0"; // refused last time
        s.run({ State::Missing, "" });
        expect(s.installs == 0 && s.host.alerts.empty() && s.provider.step() == Step::NotInstalled,
               "refused for this version: not asked again");
    }
    {
        Setup s("0.8.1");
        s.host.store[ServiceHost::kAskedKey] = "0.8.0";
        s.run({ State::Missing, "" });
        expect(s.installs == 1, "after the next update, asked once more");
    }
    {
        Setup s("0.8.0");
        s.installStarts = false;
        s.run({ State::Missing, "" });
        expect(s.provider.step() == Step::NotInstalled && s.host.store[ServiceHost::kAskedKey] == "0.8.0",
               "an installer that won't start counts as asked");
    }
    {
        Setup s("0.8.0");
        s.run({ State::Running, "0.7.9" });
        expect(s.updates == 1 && s.installs == 0 && s.provider.step() == Step::Updating, "an older service is asked to update");
        expect(!s.sent.empty() && s.sent.back().find(R"("installer":"C:\\Program\\CupIsland-Index-Setup.exe")") != std::string::npos,
               "with where the app's copy of the installer is (decision 081)");
        expect(s.host.wake == s.host.clock + tokens().serviceUpdateRetry, "and looked at again later: did it change?");
        auto again = [&](const char* version) {
            s.host.clock = *s.host.wake;
            s.provider.onWake();
            s.probes.back()({ State::Running, version });
        };
        again("0.7.9");
        again("0.7.9");
        expect(s.updates == 3 && s.host.wake, "still old: asked again, up to serviceUpdateTries");
        again("0.7.9");
        expect(s.updates == 3 && s.provider.step() == Step::UpdateFailed && !s.host.wake, "then it gives up until the next start");
        const Panel* stuck = nullptr;
        for (const Panel& p : s.host.published)
            if (p.id == ServiceHost::kPanelId) stuck = &p;
        expect(stuck && stuck->content.label == "File search needs an update", "and says so, on a widget");
        expect(!s.host.alerts.empty() && s.host.alerts.back().activityId == ServiceHost::kPanelId &&
                   s.host.store[ServiceHost::kStuckKey] == "0.8.0", "with an alert that opens it, once per app version");
        s.provider.onAction({ ServiceHost::kId, ServiceHost::kPanelId, service_action::kDownload, 0, "" });
        expect(s.opened == std::vector<std::string>{ kLatestReleasePage }, "its button opens the latest release, to download by hand");
        Setup fixed("0.8.0");
        fixed.run({ State::Running, "0.7.9" });
        fixed.host.clock = *fixed.host.wake;
        fixed.provider.onWake();
        fixed.probes.back()({ State::Running, "0.8.0" });
        expect(fixed.updates == 1 && fixed.provider.step() == Step::UpToDate && !fixed.host.wake, "updated: nothing more to do");
        expect(!fixed.host.removed.empty() && fixed.host.removed.back() == ServiceHost::kPanelId, "and no widget saying it's stuck");
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
        expect(dev.updates == 1, "an installed dev build of the service moves to the release too");
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
        auto provider = std::make_unique<ServiceHost>(ServiceHostHooks{
            .appVersion = "0.8.0",
            .installerBundled = true,
            .probe = [&](Probed p) { late = std::move(p); },
            .runInstaller = [&](std::function<void(bool)> d) { ++installs; d(true); },
            .send = [](std::string, Replied) {} });
        provider->start(host);
        host.clock = *host.wake;
        provider->onWake();
        provider.reset();
        late({ State::Missing, "" });
        expect(installs == 0, "a probe answering after the provider is gone does nothing");
    }
}

// A provider that calls the service through its Host, as search will.
struct Caller : Provider {
    std::string name;
    Host* host = nullptr;
    std::vector<ServiceReply> got;
    explicit Caller(std::string id) : name(std::move(id)) {}
    const char* id() const override { return name.c_str(); }
    void start(Host& h) override { host = &h; }
    void stop() override {}
    void onAction(const Action&) override {}
    void ask(const char* method) { host->callService(method, "{}", [this](ServiceReply r) { got.push_back(std::move(r)); }); }
};

void callTests()
{
    using Problem = ServiceReply::Problem;
    {
        Setup s("0.8.0");
        bool answered = false;
        s.provider.call("index.query", R"({"text":"a","limit":5})", [&](ServiceReply r) { answered = r.body == "{}"; });
        expect(s.sent.size() == 1 && s.sent[0].find("\"method\":\"index.query\"") != std::string::npos &&
               s.sent[0].find(R"("params":{"text":"a","limit":5})") != std::string::npos, "a call goes over the pipe as JSON-RPC");
        s.replies[0]({ Problem::None, "{}" });
        expect(answered, "and its answer comes back");
        s.provider.call("index.status", "", [](ServiceReply) {});
        expect(s.sent.size() == 2 && s.sent[1].find("\"id\":2") != std::string::npos, "each call its own id");
    }
    {
        Setup s("0.8.0");
        s.run({ ServiceProbe::State::Missing, "" });
        s.installStarts = false;
        Setup refused("0.8.0");
        refused.installStarts = false;
        refused.run({ ServiceProbe::State::Missing, "" });
        Problem problem = Problem::None;
        refused.provider.call("index.query", "{}", [&](ServiceReply r) { problem = r.problem; });
        expect(problem == Problem::NotInstalled && refused.sent.empty(), "not installed: answered at once, nothing sent");
        std::size_t before = s.sent.size();
        s.provider.call("index.query", "{}", [](ServiceReply) {});
        expect(s.sent.size() == before + 1, "an installer on its way: calls try again");
    }
    {
        // Through the registry: only granted modules get through.
        ProviderRegistry registry{ [](const Event&, const char*) {}, [] { return Ms{ 0 }; } };
        std::vector<std::string> carried;
        registry.setServiceCaller([&](const std::string& method, const std::string&, std::function<void(ServiceReply)> done) {
            carried.push_back(method);
            done({ Problem::None, "ok" });
        });
        auto search = std::make_unique<Caller>("search");
        auto other = std::make_unique<Caller>("other");
        Caller* s = search.get();
        Caller* o = other.get();
        registry.add(std::move(search));
        registry.add(std::move(other));
        registry.grant("search", "index");
        registry.start("search");
        registry.start("other");
        s->ask("index.query");
        o->ask("index.query");
        s->ask("service.update");
        s->ask("indexquery");
        expect(carried == std::vector<std::string>{ "index.query" }, "only a granted module's call is carried");
        expect(s->got.size() == 3 && s->got[0].body == "ok" && s->got[1].problem == Problem::NotAllowed &&
               s->got[2].problem == Problem::NotAllowed, "service.update is nobody's; a method needs its module");
        expect(o->got.size() == 1 && o->got[0].problem == Problem::NotAllowed, "a provider without the grant is refused");

        // An answer that arrives after the provider stopped goes nowhere.
        std::function<void(ServiceReply)> late;
        registry.setServiceCaller([&](const std::string&, const std::string&, std::function<void(ServiceReply)> done) { late = done; });
        s->ask("index.query");
        registry.stop("search");
        late({ Problem::None, "late" });
        expect(s->got.size() == 3, "a stopped provider gets no answer");
    }
    {
        ProviderRegistry registry{ [](const Event&, const char*) {}, [] { return Ms{ 0 }; } };
        auto search = std::make_unique<Caller>("search");
        Caller* s = search.get();
        registry.add(std::move(search));
        registry.grant("search", "index");
        registry.start("search");
        s->ask("index.query");
        expect(s->got.size() == 1 && s->got[0].problem == Problem::NotInstalled, "no service host at all: not installed");
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

    // Decision 081: the hash in the notes, the installer in the app's.
    std::string hash(64, 'd');
    std::string bare = R"({"tag_name":"v0.9.0","html_url":"https://github.com/Cuppie24/cup-island/releases/tag/v0.9.0",
        "body":"What's new <!-- cup-island: index-sha256 )" + std::string(64, 'D') + R"( -->","assets":[
        {"name":"CupIsland-Setup-0.9.0.exe","browser_download_url":"https://github.com/Cuppie24/cup-island/releases/download/v0.9.0/CupIsland-Setup-0.9.0.exe","digest":"sha256:)" +
                       std::string(64, 'a') + R"("}]})";
    auto only = parseReleaseJson(bare);
    expect(only && only->indexSha256 == hash && !only->mandatory, "the service installer's hash, from the notes");
    auto fromNotes = only ? indexOfferFor({ 0, 8, 0 }, *only) : std::nullopt;
    expect(fromNotes && fromNotes->sha256 == hash && fromNotes->url.empty() && fromNotes->fileName == "CupIsland-Index-Setup-0.9.0.exe",
           "no attached file: an offer with the notes' hash and nothing to download");
    GitHubRelease both = *release;
    both.indexSha256 = hash;
    auto preferred = indexOfferFor({ 0, 7, 9 }, both);
    expect(preferred && preferred->sha256 == hash && !preferred->url.empty(), "both: the notes' hash, and the file to fall back on");
    GitHubRelease neither = *only;
    neither.indexSha256.clear();
    expect(!indexOfferFor({ 0, 8, 0 }, neither), "no hash anywhere: nothing");
    expect(!indexHashIn("<!-- cup-island: index-sha256 abc -->") && !indexHashIn("<!-- cup-island: index-sha256 " + hash) &&
               !indexHashIn("nothing here"), "a short, unclosed or missing marker is none");

    expect(plainLocalPath("C:\\Users\\me\\AppData\\Local\\Programs\\Cup Island\\CupIsland-Index-Setup.exe"), "a local path");
    expect(!plainLocalPath("\\\\server\\share\\x.exe") && !plainLocalPath("\\\\?\\C:\\x.exe") && !plainLocalPath("\\\\.\\pipe\\x"),
           "never a network, device or \\\\?\\ path");
    expect(!plainLocalPath("C:\\x.exe:stream") && !plainLocalPath("C:x.exe") && !plainLocalPath("C:/x.exe") &&
               !plainLocalPath("C:\\a\\\\b.exe") && !plainLocalPath("x.exe"), "nor a stream, a relative, mixed or empty-part one");
    expect(!parseReleaseJson("[]") && !parseReleaseJson("nope"), "not a release");
}

} // namespace

void runServiceHostTests()
{
    providerTests();
    callTests();
    releaseTests();
}

} // namespace cup::test
