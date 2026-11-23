// Update and restart installs the latest release as it is at the click
// (decision 172), not the one a check found earlier: another may have come
// out since, even right after a check. The updater's other tests:
// update_tests.cpp.

#include "expect.h"
#include "update_fixture.h"

#include <utility>

namespace cup::test {

void runUpdateInstallTests()
{
    {
        Updater n("0.3.0");
        n.provider.start(n.host);
        n.wakeAndAnswer(releaseOf("0.4.0"));
        n.installWith(releaseOf("0.4.1"));
        expect(n.installed == std::vector<std::string>{ "0.4.1" }, "at install: a release out since the check is the one installed");
        expect(n.status && n.status->version == "0.4.1" && n.status->stage == UpdateStage::Installing &&
                   n.status->page.find("/v0.4.1") != std::string::npos, "at install: Settings shows that one installing");
        expect(n.host.load(UpdateProvider::kOfferKey).rfind("0.4.1\n", 0) == 0, "at install: and it's the offer saved");
        n.installs.back()("Couldn't download the update");
        expect(n.status->stage == UpdateStage::Failed && n.status->version == "0.4.1", "at install: a failure is that one's");
        n.provider.stop();
    }
    {
        Updater pulled("0.3.0"); // 0.5.0 was pulled: the latest is 0.4.0 again
        pulled.provider.start(pulled.host);
        pulled.wakeAndAnswer(releaseOf("0.5.0"));
        pulled.installWith(releaseOf("0.4.0"));
        expect(pulled.installed == std::vector<std::string>{ "0.4.0" }, "at install: a lower latest too (decision 040)");
        pulled.provider.stop();
    }
    {
        Updater offline("0.3.0");
        offline.provider.start(offline.host);
        offline.wakeAndAnswer(releaseOf("0.4.0"));
        offline.installWith(std::nullopt);
        expect(offline.installed == std::vector<std::string>{ "0.4.0" } && offline.status->stage == UpdateStage::Installing,
               "at install: GitHub can't be asked: the offer as found");
        offline.provider.stop();
    }
    {
        Updater gone("0.3.0"); // 0.4.0 pulled, and this copy runs the latest again
        gone.provider.start(gone.host);
        gone.wakeAndAnswer(releaseOf("0.4.0"));
        gone.installWith(releaseOf("0.3.0"));
        expect(gone.installs.empty() && !gone.status && gone.host.load(UpdateProvider::kOfferKey).empty(),
               "at install: nothing to move to any more: withdrawn, nothing installed");
        gone.provider.stop();
    }
    {
        Updater both("0.3.0");
        std::vector<std::pair<CheckOutcome, std::string>> said;
        both.provider.start(both.host);
        both.wakeAndAnswer(releaseOf("0.4.0"));
        both.provider.checkNow([&](CheckOutcome o, const std::string& v) { said.push_back({ o, v }); });
        std::size_t asked = both.fetches.size();
        both.provider.install();
        expect(both.fetches.size() == asked && both.installs.empty(), "at install: a check under way answers for it, no second request");
        both.fetches.back()(releaseOf("0.4.2"));
        expect(both.installed == std::vector<std::string>{ "0.4.2" } && said.size() == 1 && said[0].second == "0.4.2",
               "at install: one answer, both told: installed, and Check for updates says which");
        both.provider.stop();
    }
    {
        Updater late("0.3.0");
        late.provider.start(late.host);
        late.wakeAndAnswer(releaseOf("0.4.0"));
        late.provider.install();
        late.provider.stop();
        late.fetches.back()(releaseOf("0.4.1"));
        expect(late.installs.empty(), "at install: stopped before GitHub answered: nothing installs");
    }
    {
        GitHubRelease required = releaseOf("0.6.0");
        required.mandatory = true;
        Updater m("0.5.3");
        m.provider.start(m.host);
        m.wakeAndAnswer(required);
        m.provider.stop();
        m.provider.start(m.host);
        m.fetches.back()(releaseOf("0.6.1"));
        expect(m.installed == std::vector<std::string>{ "0.6.1" } && m.host.alerts.back().content.alertCenter[1].items.size() == 1 &&
                   std::get<Text>(m.host.alerts.back().content.alertCenter[1].items[0]).value.find("0.6.1") != std::string::npos,
               "at install: a mandatory one out of date by the next start: the latest installs, and the alert names it");
        m.provider.stop();
    }
}

} // namespace cup::test
