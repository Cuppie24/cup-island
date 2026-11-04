#pragma once
// The host's side of Cup Island's service (decisions 076, 078): the one part
// of the island that may set it up, update it and talk to it. Providers reach
// it only through Host::callService, which the registry checks against their
// grants and hands to call() here.
//   - searchSetupDelay after start it looks for the service: missing, it
//     runs the installer bundled beside the app (one UAC prompt, said first;
//     refused, not asked again until the app's next version); running an
//     older or other release, it sends service.update, silently.
//   - call() carries a provider's request over the pipe; "not installed"
//     is answered at once once that's known.
// Nothing for a dev build, or without a bundled installer (calls still go
// through, to a service run by hand). Shows nothing: no widget, no setting.
// The pipe, Windows and the installer are the app's (ServiceHostHooks), so
// this is pure and tested on Linux.

#include "core/provider.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace cup {

// What the app found out about the service.
struct ServiceProbe {
    enum class State {
        Missing,  // not installed
        Stopped,  // installed but not answering (stopped, starting, crashed: Windows restarts it)
        Running,  // answered index.status
    };
    State state = State::Missing;
    std::string version; // Running: the service's own ("0.8.0")
};

struct ServiceHostHooks {
    std::string appVersion;        // cup_version.h: only an X.Y.Z build sets anything up
    bool installerBundled = false; // CupIsland-Index-Setup.exe is beside the app
    // Asks Windows and the service; `done` on the UI thread.
    std::function<void(std::function<void(ServiceProbe)> done)> probe;
    // Starts the bundled installer; Windows asks the user first (so this
    // runs off the UI thread). `done` on the UI thread: false if it couldn't
    // start or was refused.
    std::function<void(std::function<void(bool started)> done)> runInstaller;
    // Sends one JSON-RPC request to the installed service (and only it:
    // index_client checks the pipe's server); `done` on the UI thread.
    std::function<void(std::string request, std::function<void(ServiceReply)> done)> send;
    // A line for the app's log: what it found and did, so a stuck update can be reported.
    std::function<void(const std::string&)> log;
};

class ServiceHost : public Provider {
public:
    explicit ServiceHost(ServiceHostHooks hooks) : hooks_(std::move(hooks)) {}
    ~ServiceHost() override { *alive_ = false; } // answers still on their way call into nothing

    static constexpr const char* kId = "system.service";
    static constexpr const char* kAskedKey = "asked-for"; // the app version that last set it up

    const char* id() const override { return kId; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action&) override {}
    void onWake() override;

    // A provider's Host::callService, already checked against its grants.
    void call(const std::string& method, const std::string& params, std::function<void(ServiceReply)> done);

    // For tests and the log: what the last look led to.
    enum class Step { None, Waiting, Installing, NotInstalled, Updating, UpToDate, UpdateFailed };
    Step step() const { return step_; }

private:
    void found(const ServiceProbe& probe);
    void log(const std::string& line);

    ServiceHostHooks hooks_;
    Host* host_ = nullptr;
    Step step_ = Step::None;
    bool missing_ = false; // the last look found no service
    std::size_t updateAsks_ = 0; // service.update sent this start
    std::int64_t nextId_ = 1;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace cup
