// Providers' commands in Search (decision 160): their rows, where they rank,
// running one through the host, and, end to end through the registry,
// Pomodoro's window opening from Search's Enter.

#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"
#include "providers/search/result_menu.h"
#include "providers/search/search_provider.h"

namespace cup::test {
namespace {

using Kind = Launchable::Kind;

// What the registry would list, and what a run would come to.
struct CommandHost : PanelHost {
    std::vector<PublishedCommand> listed;
    CommandRun answer = CommandRun::Ran;
    std::vector<std::string> ran; // "<provider>/<id>"
    std::vector<std::string> closed;
    std::vector<PublishedCommand> listCommands() const override { return listed; }
    CommandRun runCommand(const std::string& providerId, const std::string& commandId) override
    {
        ran.push_back(providerId + "/" + commandId);
        return answer;
    }
    void closeCard(const std::string& id) override { closed.push_back(id); }
};

PublishedCommand command(std::string provider, std::string id, std::string title, std::vector<std::string> words = {},
                         Glyph glyph = Glyph::OpenApp)
{
    return { std::move(provider), { .id = std::move(id), .title = std::move(title), .keywords = std::move(words), .glyph = glyph } };
}

void targetTests()
{
    expect(commandTarget("system.theme", "toggle") == "command:system.theme/toggle", "a command's target names it");
    expect(commandOf("command:system.theme/toggle") == CommandRef{ "system.theme", "toggle" }, "...and reads back");
    expect(!commandOf("ms-settings:display") && !commandOf("command:/x") && !commandOf("command:x/") && !commandOf("command:x"),
           "anything else isn't one");
}

void rowTests()
{
    CommandHost host;
    host.listed = { command("system.theme", "toggle", "Switch Windows to dark mode", { "theme", "dark mode" }, Glyph::Moon),
                    command("pomodoro", "window.open", "Open the Pomodoro timer", { "pomodoro", "timer" }, Glyph::Timer) };
    SearchProvider search({ .loadApps = [](std::function<void(std::vector<Launchable>)> d) { d({ { .name = "Dark Reader", .target = "reader" } }); },
                            .windowsBuild = 22621 });
    search.start(host);
    auto type = [&](const std::string& text) { search.onAction({ "search", "search", search_action::kTyped, 0, text }); };

    type("dark");
    auto r = search.results();
    expect(r.size() >= 3 && r[0].name == "Dark Reader" && r[1].name == "Switch Windows to dark mode" && r[2].kind == Kind::Setting,
           "apps, then the island's commands, then Windows' pages");
    expect(r.size() >= 2 && r[1].kind == Kind::Command && r[1].caption == "Cup Island" && r[1].glyph == Glyph::Moon,
           "a command says it's the island's, with its provider's icon");
    const Row* found = nullptr;
    for (const Row& each : host.published.back().content.expandedCenter)
        if (each.items.size() > 1 && std::holds_alternative<Text>(each.items[1]) && std::get<Text>(each.items[1]).value == r[1].name) found = &each;
    expect(found != nullptr, "the command has a row");
    if (!found) return;
    const Row& row = *found;
    auto* icon = std::get_if<Button>(&row.items[0]);
    expect(icon && icon->glyph == Glyph::Moon && !icon->picture, "its row: the glyph, no icon asked of Windows");
    std::vector<std::string> labels;
    for (const MenuItem& m : row.menu) labels.push_back(m.label);
    expect(labels == std::vector<std::string>{ "Open" }, "its menu: Open alone (nothing to reveal, copy or uninstall)");

    type("pomo");
    expect(search.results().size() == 1 && search.results()[0].name == "Open the Pomodoro timer", "a keyword matches too");

    // Refused (its provider went meanwhile): the card stays, nothing remembered.
    host.answer = CommandRun::Unknown;
    search.onAction({ "search", "search", search_action::kGo, 0, "pomo" });
    expect(host.ran == std::vector<std::string>{ "pomodoro/window.open" } && host.closed.empty() &&
               host.store[SearchProvider::kHistoryKey].empty(),
           "refused: the card stays open, nothing is remembered");
    // Ran: out of the way, and remembered like an app.
    host.answer = CommandRun::Ran;
    search.onAction({ "search", "search", search_action::kGo, 0, "pomo" });
    expect(host.ran.size() == 2 && host.closed.size() == 1 &&
               host.store[SearchProvider::kHistoryKey].find("command:pomodoro/window.open") != std::string::npos,
           "Enter runs the top command through the host; the card closes, and it's remembered");
    type("");
    type("o");
    expect(!search.results().empty() && search.results()[0].remembered, "...and offered first next time, with Remove from history");

    // The list follows what the host has now: the theme switched, its name did.
    host.listed[0] = command("system.theme", "toggle", "Switch Windows to light mode", { "theme", "light mode" }, Glyph::Sun);
    type("light mode");
    expect(!search.results().empty() && search.results()[0].name == "Switch Windows to light mode", "listed again at every search");

    // At most searchCommandRows, so commands never push the pages off the card.
    host.listed.clear();
    for (int i = 0; i < 6; ++i) host.listed.push_back(command("p", "c" + std::to_string(i), "Zap " + std::to_string(i)));
    type("zap");
    expect(search.results().size() == tokens().searchCommandRows, "at most searchCommandRows commands");
}

// The real thing: Search granted commands, Pomodoro publishing its window.
void registryTests()
{
    Ms now{ 0 };
    ProviderRegistry registry{ [](const Event&, const char*) {}, [&] { return now; } };
    auto owned = std::make_unique<SearchProvider>(SearchHooks{ .loadApps = [](std::function<void(std::vector<Launchable>)> d) { d({}); } });
    SearchProvider& search = *owned;
    registry.add(std::move(owned));
    registry.add(std::make_unique<PomodoroProvider>());
    registry.start("pomodoro");
    registry.start(SearchProvider::kId);

    registry.route({ "search", "search", search_action::kTyped, 0, "pomodoro" });
    expect(search.results().empty(), "not granted: no commands");

    registry.grant(SearchProvider::kId, ProviderRegistry::kCommandsGrant);
    now = Ms{ 10 };
    registry.route({ "search", "search", search_action::kTyped, 0, "pomodoro" });
    expect(search.results().size() == 1 && search.results()[0].name == "Open the Pomodoro timer", "granted: Pomodoro's command shows");
    now = Ms{ 20 };
    registry.route({ "search", "search", search_action::kGo, 0, "pomodoro" });
    expect(registry.windows().open.size() == 1 && registry.windows().open[0].key.providerId == "pomodoro",
           "Enter opens the timer's window: the user's key passed on to Pomodoro");
}

} // namespace

void runSearchCommandTests()
{
    targetTests();
    rowTests();
    registryTests();
}

} // namespace cup::test
