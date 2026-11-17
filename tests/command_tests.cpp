// Providers' commands through the registry (decision 154): who may list and
// run them, and the real providers' commands doing what their buttons do.
// The run rules themselves: scenarios/commands.scenario.

#include "expect.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"
#include "providers/system/settings_provider.h"

#include <algorithm>

namespace cup::test {
namespace {

// Search's part, without Search: lists commands, and runs one when clicked.
struct Runner : Provider {
    std::string name;
    Host* host = nullptr;
    std::string runProvider, runCommand; // what a click runs
    CommandRun last = CommandRun::Unknown;
    explicit Runner(std::string id) : name(std::move(id)) {}
    const char* id() const override { return name.c_str(); }
    void start(Host& h) override
    {
        host = &h;
        h.publishCommands({ { .id = "own", .title = "Its own" } });
    }
    void stop() override {}
    void onAction(const Action&) override { last = host->runCommand(runProvider, runCommand); }
    void onWake() override { last = host->runCommand(runProvider, runCommand); } // not the user: refused
};

bool listed(const std::vector<PublishedCommand>& list, const std::string& provider, const std::string& id)
{
    return std::any_of(list.begin(), list.end(), [&](const PublishedCommand& c) { return c.providerId == provider && c.spec.id == id; });
}

} // namespace

void runCommandTests()
{
    Ms now{ 0 };
    ProviderRegistry registry{ [](const Event&, const char*) {}, [&] { return now; } };
    auto owned = std::make_unique<Runner>("search");
    Runner& search = *owned;
    registry.add(std::move(owned));
    auto other = std::make_unique<Runner>("other"); // not granted
    Runner& stranger = *other;
    registry.add(std::move(other));
    registry.add(std::make_unique<PomodoroProvider>());
    registry.add(std::make_unique<SettingsProvider>(SettingsHooks{}));
    registry.grant("search", ProviderRegistry::kCommandsGrant);
    for (const char* id : { "search", "other", "pomodoro", "system.settings" }) registry.start(id);

    // Published: the first ones, and only a granted provider sees them, without its own.
    std::vector<PublishedCommand> list = search.host->listCommands();
    expect(listed(list, "pomodoro", "window.open") && listed(list, "system.settings", "settings.window"),
           "Pomodoro and Settings publish their windows as commands");
    expect(!listed(list, "search", "own"), "a provider's own commands aren't listed back to it");
    expect(stranger.host->listCommands().empty(), "not granted: nothing listed");

    // Not granted: refused before anything runs.
    stranger.runProvider = "pomodoro", stranger.runCommand = "window.open";
    registry.route({ "other", "card", "click" });
    expect(stranger.last == CommandRun::NotAllowed && registry.windows().open.empty(), "not granted: refused");

    // Not the user (a wake-up): refused, as it would be after a click long ago.
    search.runProvider = "pomodoro", search.runCommand = "window.open";
    search.onWake();
    expect(search.last == CommandRun::NotNow && registry.windows().open.empty(), "not right after the user: refused");

    // A click on Search runs Pomodoro's: its window opens, as from its expand button.
    registry.route({ "search", "card", "click" });
    expect(search.last == CommandRun::Ran && registry.windows().open.size() == 1 &&
               registry.windows().open[0].key == WindowKey{ "pomodoro", PomodoroProvider::kWindowId },
           "Open the Pomodoro timer: its window opens");

    // The same click doesn't run a second; the next click runs Settings'.
    search.runProvider = "system.settings", search.runCommand = "settings.window";
    search.onAction({});
    expect(search.last == CommandRun::NotNow && registry.windows().open.size() == 1, "one click, one command");
    now = Ms{ 10 };
    registry.route({ "search", "card", "click" });
    expect(search.last == CommandRun::Ran && registry.windows().open.size() == 2 &&
               registry.windows().open[1].key.providerId == "system.settings",
           "Cup Island settings: its window opens");

    // Granted or not, the module is the host's, never the service's.
    ServiceReply reply;
    search.host->callService("commands.list", "{}", [&](ServiceReply r) { reply = std::move(r); });
    expect(reply.problem == ServiceReply::Problem::NotAllowed, "the commands grant isn't a service module");

    // A provider that stops takes its commands with it.
    registry.stop("pomodoro");
    expect(!listed(search.host->listCommands(), "pomodoro", "window.open"), "stopped: its commands go");
    search.runProvider = "pomodoro", search.runCommand = "window.open";
    now = Ms{ 20 };
    registry.route({ "search", "card", "click" });
    expect(search.last == CommandRun::Unknown, "stopped: nothing to run");
}

} // namespace cup::test
