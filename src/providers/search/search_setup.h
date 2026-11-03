#pragma once
// Sets up file search and keeps it current (decision 076). Once per start,
// searchSetupDelay after it, the app looks for the service:
//   - missing: run the installer bundled beside the app (Windows asks for
//     permission: one UAC prompt), saying so first; refused, it isn't asked
//     again until the app's next version;
//   - running an older or other release: ask it to update itself, silently.
// Nothing for a dev build, or without a bundled installer. Shows nothing
// otherwise: no widget, no setting. Asking Windows and the service, and
// starting the installer, are the app's (SearchSetupHooks), so this is pure
// and tested on Linux.

#include "core/provider.h"
#include "indexer/protocol.h"

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
        Running,  // answered status
    };
    State state = State::Missing;
    std::string version; // Running: the service's own ("0.8.0")
};

struct SearchSetupHooks {
    std::string appVersion;       // cup_version.h: only an X.Y.Z build sets anything up
    bool installerBundled = false; // CupIsland-Index-Setup.exe is beside the app
    // Asks Windows and the service, then calls `done` on the UI thread.
    std::function<void(std::function<void(ServiceProbe)> done)> probe;
    // Starts the bundled installer; Windows asks the user first. `done` on
    // the UI thread: true if it started, false if it couldn't or was refused.
    // Off the UI thread: the call waits for the user's answer.
    std::function<void(std::function<void(bool started)> done)> runInstaller;
    // Sends the service `update`; `done` on the UI thread with its answer, or nothing.
    std::function<void(std::function<void(std::optional<indexer::UpdateAnswer>)> done)> askUpdate;
};

class SearchSetupProvider : public Provider {
public:
    explicit SearchSetupProvider(SearchSetupHooks hooks) : hooks_(std::move(hooks)) {}
    ~SearchSetupProvider() override { *alive_ = false; } // a probe still running calls back into nothing

    static constexpr const char* kAskedKey = "asked-for"; // the app version that last set it up

    const char* id() const override { return "system.search-setup"; }
    void start(Host& host) override;
    void stop() override;
    void onAction(const Action&) override {}
    void onWake() override;

    // For tests and the log: what the last probe led to.
    enum class Step { None, Waiting, Installing, NotInstalled, Updating, UpToDate };
    Step step() const { return step_; }

private:
    void found(const ServiceProbe& probe);

    SearchSetupHooks hooks_;
    Host* host_ = nullptr;
    Step step_ = Step::None;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace cup
