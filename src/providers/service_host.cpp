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
    host_->wakeAt(std::nullopt); // once per start, unless an update asks to look again
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

void ServiceHost::log(const std::string& line)
{
    if (hooks_.log) hooks_.log(line);
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
        // A release app asks whenever the versions differ, a dev-built service
        // included: the pipe is the installed service's (index_client checks),
        // so it was installed, and follows releases like any other. One run by
        // hand (--console) never answers here. (It used to be left alone, so a
        // service installed from a local build never moved to a release.)
        if (!app || (service && !movesTo(*service, *app))) {
            step_ = Step::UpToDate;
            return;
        }
        if (updateAsks_ >= tokens().serviceUpdateTries) {
            step_ = Step::UpdateFailed;
            log("service: still " + probe.version + " after " + std::to_string(updateAsks_) +
                " update requests; trying again at the next start (its own log says why)");
            return;
        }
        ++updateAsks_;
        step_ = Step::Updating;
        log("service: " + probe.version + ", the app is " + hooks_.appVersion + ": asking it to update (try " +
            std::to_string(updateAsks_) + " of " + std::to_string(tokens().serviceUpdateTries) + ")");
        hooks_.send(indexer::encodeUpdateRequest(nextId_++, hooks_.installerPath), [](ServiceReply) {}); // the app logs the answer (service_link)
        host_->wakeAt(host_->now() + tokens().serviceUpdateRetry); // then look again: did it change?
        return;
    }
    }
}

} // namespace cup
