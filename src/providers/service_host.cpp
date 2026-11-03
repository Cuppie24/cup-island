#include "providers/service_host.h"

#include "core/tokens.h"
#include "indexer/protocol.h"
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

void ServiceHost::start(Host& host)
{
    host_ = &host;
    // A dev build, or no installer to run: nothing to set up, and a dev
    // build's service (run by hand) isn't ours to update. Calls still work.
    if (!parseVersion(hooks_.appVersion) || !hooks_.installerBundled) return;
    step_ = Step::Waiting;
    host.wakeAt(host.now() + tokens().searchSetupDelay);
}

void ServiceHost::stop()
{
    if (host_) host_->wakeAt(std::nullopt);
}

void ServiceHost::onWake()
{
    host_->wakeAt(std::nullopt); // once per start
    hooks_.probe([this, alive = alive_](ServiceProbe probe) {
        if (*alive) found(probe);
    });
}

void ServiceHost::call(const std::string& method, const std::string& params, std::function<void(ServiceReply)> done)
{
    if (missing_) {
        done({ ServiceReply::Problem::NotInstalled, {} });
        return;
    }
    hooks_.send(indexer::encodeCall(nextId_++, method, params), [alive = alive_, done = std::move(done)](ServiceReply reply) {
        if (*alive) done(std::move(reply));
    });
}

void ServiceHost::found(const ServiceProbe& probe)
{
    missing_ = probe.state == ServiceProbe::State::Missing;
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
            if (!*alive) return;
            if (started) missing_ = false; // it's on its way: let calls try
            else step_ = Step::NotInstalled;
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
        hooks_.send(indexer::encodeUpdateRequest(nextId_++), [](ServiceReply) {}); // the service logs what it did
        return;
    }
    }
}

} // namespace cup
