#include "providers/search/search_setup.h"

#include "core/tokens.h"
#include "providers/update/release.h"

namespace cup {
namespace {

Alert settingUpAlert()
{
    return { .content = { .label = "Setting up file search",
                          .alertLeading = { row({ Icon{ Glyph::Download } }) },
                          .alertCenter = { row({ Text{ "Setting up file search", TextStyle::Title } }),
                                           row({ Text{ "Windows will ask for permission", TextStyle::Caption } }) } },
             .coalesceKey = "search-setup" };
}

} // namespace

void SearchSetupProvider::start(Host& host)
{
    host_ = &host;
    // A dev build, or no installer to run: nothing to set up, and a dev
    // build's service (run by hand) isn't ours to update.
    if (!parseVersion(hooks_.appVersion) || !hooks_.installerBundled) return;
    step_ = Step::Waiting;
    host.wakeAt(host.now() + tokens().searchSetupDelay);
}

void SearchSetupProvider::stop()
{
    if (host_) host_->wakeAt(std::nullopt);
}

void SearchSetupProvider::onWake()
{
    host_->wakeAt(std::nullopt); // once per start
    auto alive = alive_;
    hooks_.probe([this, alive](ServiceProbe probe) {
        if (*alive) found(probe);
    });
}

void SearchSetupProvider::found(const ServiceProbe& probe)
{
    switch (probe.state) {
    case ServiceProbe::State::Missing:
        // Once per app version: a refused prompt isn't repeated at every
        // start, only after the next update (decision 076).
        if (host_->load(kAskedKey) == hooks_.appVersion) {
            step_ = Step::NotInstalled;
            return;
        }
        host_->save(kAskedKey, hooks_.appVersion);
        host_->postAlert(settingUpAlert());
        step_ = Step::Installing;
        hooks_.runInstaller([this, alive = alive_](bool started) {
            if (*alive && !started) step_ = Step::NotInstalled;
        });
        return;
    case ServiceProbe::State::Stopped:
        step_ = Step::None; // Windows restarts it; nothing for the app to do
        return;
    case ServiceProbe::State::Running: {
        auto service = parseVersion(probe.version);
        auto app = parseVersion(hooks_.appVersion);
        // A dev build of the service (run by hand) is left alone.
        if (!service || !app || !movesTo(*service, *app)) {
            step_ = Step::UpToDate;
            return;
        }
        step_ = Step::Updating;
        hooks_.askUpdate([](std::optional<indexer::UpdateAnswer>) {}); // the service logs what it did
        return;
    }
    }
}

} // namespace cup
