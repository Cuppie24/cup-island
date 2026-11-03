// Search (decisions 077, 079): what's kept from Windows' app list, how apps
// and Settings pages rank, what was launched before, the card's results as
// you type (files through the host), Enter and the card closing after; and
// the live field that sends every edit.

#include "core/card_overlays.h"
#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/search/search_provider.h"

namespace cup::test {
namespace {

using Kind = Launchable::Kind;

Launchable app(std::string name, std::string target, Kind kind = Kind::App)
{
    return { kind, std::move(name), std::move(target), {}, nullptr };
}

void cleanupTests()
{
    auto kept = cleanedApps({ app("Visual Studio Code", "Microsoft.VisualStudioCode"), app("Uninstall Foo", "C:\\Foo\\unins000.exe"),
                              app("Удалить Bar", "C:\\Bar\\uninst.exe"), app("Foo Help", "C:\\Foo\\help.chm"),
                              app("Foo Website", "https://foo.example"), app("Readme", "C:\\Foo\\README.TXT"),
                              app("Code again", "microsoft.visualstudiocode"), app("", "x"), app("Calculator", "Microsoft.WindowsCalculator_8wekyb3d8bbwe!App") });
    expect(kept.size() == 2 && kept[0].name == "Visual Studio Code" && kept[1].name == "Calculator",
           "uninstallers, help files, web links, documents and duplicates go");
}

void settingsTests()
{
    auto ten = settingsPages(17763, false);
    auto eleven = settingsPages(22621, false);
    auto byLink = [](const std::vector<Launchable>& pages, std::string_view link) {
        for (const Launchable& p : pages)
            if (p.target == link) return &p;
        return static_cast<const Launchable*>(nullptr);
    };
    expect(byLink(ten, "ms-settings:network-wifi") && byLink(ten, "ms-settings:network-wifi")->kind == Kind::Setting, "Settings pages, as links");
    expect(!byLink(ten, "ms-settings:apps-volume") && byLink(eleven, "ms-settings:apps-volume"), "a Windows 11 page only on Windows 11");
    auto ru = settingsPages(17763, true);
    expect(byLink(ru, "ms-settings:bluetooth")->name == "Bluetooth и устройства", "named in Russian on a Russian system");
    expect(launchScore(*byLink(ru, "ms-settings:windowsupdate"), "update") > 0 &&
           launchScore(*byLink(ten, "ms-settings:windowsupdate"), "обновление") > 0, "...and both names match either way");
    expect(launchScore(*byLink(ten, "ms-settings:network-wifi"), "вайфай") > 0 && launchScore(*byLink(ten, "ms-settings:colors"), "dark mode") > 0,
           "a page's search words match");
}

void rankingTests()
{
    expect(launchScore(app("Visual Studio Code", "x"), "vsc") == 2 && launchScore(app("Visual Studio Code", "x"), "vs") == 2,
           "initials match like a word start");
    expect(launchScore(app("ЯндексМузыка", "x"), "ям") == 2 && launchScore(app("VisualStudio", "x"), "vs") == 2,
           "a capital after a small letter starts a word: ЯндексМузыка's ям");
    expect(launchScore(app("ВКонтакте", "x"), "вк") == 3, "a name's start still beats its initials");
    expect(launchScore(app("Visual Studio Code", "x"), "code") == 2 && launchScore(app("Code", "x"), "code") == 4, "names score as files do");
    expect(launchScore(app("Visual Studio Code", "x"), "vsc ext") == 0 && launchScore(app("Calculator", "x"), "z") == 0,
           "every word must match");
    expect(launchScore(app("Visual Studio Code", "x"), "v") == 3 && launchScore(app("ab", "x"), "x") == 0,
           "one letter matches a name's start, not its initials alone");

    std::vector<Launchable> items{ app("Chrome", "chrome"), app("Chromium", "chromium"), app("Google Chrome Canary", "canary") };
    LaunchHistory none;
    Ms now{ 100LL * 24 * 3600 * 1000 };
    auto names = [&](std::vector<std::size_t> picks) {
        std::vector<std::string> out;
        for (std::size_t i : picks) out.push_back(items[i].name);
        return out;
    };
    expect(names(rankLaunchables(items, "chrom", none, now, 10)) == std::vector<std::string>{ "Chrome", "Chromium", "Google Chrome Canary" },
           "the name's start first, shorter first");
    LaunchHistory used;
    used.record("chromium", now - Ms{ 3600 * 1000 });
    expect(names(rankLaunchables(items, "chrom", used, now, 10)) == std::vector<std::string>{ "Chromium", "Chrome", "Google Chrome Canary" },
           "among equal matches, what was launched comes first");
    expect(names(rankLaunchables(items, "chrome", used, now, 10)).front() == "Chrome", "...never above a better match");
    expect(names(rankLaunchables(items, "", used, now, 10)) == std::vector<std::string>{ "Chromium" }, "nothing typed: the used ones");
    expect(rankLaunchables(items, "", none, now, 10).empty() && rankLaunchables(items, "chrom", none, now, 1).size() == 1, "and the limit");

    // History: kept as text, faded over a month, capped.
    LaunchHistory h;
    h.record("a b c", Ms{ 1000 });
    h.record("a b c", Ms{ 2000 });
    LaunchHistory back = LaunchHistory::load(h.save());
    expect(back.uses == h.uses && back.uses["a b c"].count == 2, "history round-trips, targets with spaces too");
    Ms month{ 30LL * 24 * 3600 * 1000 };
    expect(h.weight("a b c", Ms{ 2000 }) == 2 && h.weight("a b c", Ms{ 2000 } + month) == 0 && h.weight("nope", Ms{ 0 }) == 0,
           "launches fade out over a month");
    expect(LaunchHistory::load("junk\n0 5 x\n3 7\n").uses.empty(), "unreadable lines are skipped");
    LaunchHistory full;
    for (std::size_t i = 0; i < kMaxHistory + 10; ++i) full.record("t" + std::to_string(i), Ms{ static_cast<long long>(i) });
    expect(full.uses.size() == kMaxHistory && !full.uses.count("t0") && full.uses.count("t309"), "the oldest go past the cap");
}

// A host whose service calls the test answers, and which records closeCard.
struct SearchHost : PanelHost {
    std::vector<std::string> calls;
    std::vector<std::function<void(ServiceReply)>> answers;
    std::vector<std::string> closed;
    void callService(const std::string& method, const std::string& params, std::function<void(ServiceReply)> done) override
    {
        calls.push_back(method + " " + params);
        answers.push_back(std::move(done));
    }
    void closeCard(const std::string& id) override { closed.push_back(id); }
};

void providerTests()
{
    SearchHost host;
    std::function<void(std::vector<Launchable>)> appsDone;
    std::vector<std::string> launched, opened;
    SearchProvider search({ .loadApps = [&](std::function<void(std::vector<Launchable>)> d) { appsDone = std::move(d); },
                            .launch = [&](const Launchable& l) { launched.push_back(l.target); return true; },
                            .openFile = [&](const std::string& p) { opened.push_back(p); return true; },
                            .windowsBuild = 22621 });
    search.start(host);
    const Panel& first = host.published.back();
    expect(first.id == "search" && textAt(first, 0, 0) == "Search", "a Search widget on Home");
    auto* field = std::get_if<TextInput>(&first.content.expandedCenter[0].items[0]);
    expect(field && field->changeAction == search_action::kTyped && field->actionId == search_action::kGo, "its card's live field");
    expect(host.calls.empty(), "nothing typed: no files asked for");

    appsDone({ app("Visual Studio Code", "vscode"), app("Uninstall VS", "C:\\unins.exe"), app("Power Options", "::{power}", Kind::ControlPanel) });
    auto type = [&](const std::string& text) { search.onAction({ "search", "search", search_action::kTyped, 0, text }); };
    type("vsc");
    auto r = search.results();
    expect(r.size() == 1 && r[0].name == "Visual Studio Code" && r[0].caption == "App", "apps as you type");
    expect(host.calls.size() == 1 && host.calls[0] == R"(index.query {"text":"vsc","limit":8})", "files asked of the service, through the host");

    type("power");
    host.answers[0]({ ServiceReply::Problem::None, R"({"jsonrpc":"2.0","id":1,"result":{"hits":[{"name":"old.txt","path":"C:\\old.txt"}]}})" });
    expect(search.results().size() == 2 && !search.results().back().file, "an older query's files are dropped");
    host.answers[1]({ ServiceReply::Problem::None,
                      R"({"jsonrpc":"2.0","id":2,"result":{"hits":[{"name":"power.txt","path":"C:\\Docs\\power.txt","folder":false,"hidden":false}]}})" });
    r = search.results();
    expect(r.size() == 3 && r[0].name == "Power & sleep" && r[0].caption == "Settings" && r[1].name == "Power Options" &&
           r[1].caption == "Control Panel" && r[2].file && r[2].caption == "C:\\Docs", "Settings and Control Panel, then files with their folder");

    search.onAction({ "search", "search", search_action::kOpen + std::string("2"), 0, "" });
    expect(opened == std::vector<std::string>{ "C:\\Docs\\power.txt" } && host.closed.size() == 1, "a file row opens it and the card closes");
    search.onAction({ "search", "search", search_action::kGo, 0, "power" });
    expect(launched == std::vector<std::string>{ "ms-settings:powersleep" } && host.closed.size() == 2, "Enter starts the top one");
    expect(host.store[SearchProvider::kHistoryKey].find("ms-settings:powersleep") != std::string::npos, "and it's remembered");
    type("power");
    expect(search.results()[0].target == "ms-settings:powersleep" && search.results()[1].target == "::{power}", "and ranks first next time");

    type("zzz");
    host.answers.back()({ ServiceReply::Problem::NotInstalled, {} });
    expect(search.results().empty() && search.filesNote() == "File search isn't set up", "no service: said, not hidden");
    expect(cardText(host.published.back(), host.published.back().content.expandedCenter.size() - 1) == "File search isn't set up",
           "on the card");
    type("");
    expect(search.results().size() == 1 && search.results()[0].target == "ms-settings:powersleep", "nothing typed: what was launched before");
}

void liveFieldTests()
{
    Scene scene;
    TextInput live{ .actionId = "go", .changeAction = "typed" };
    Placed field;
    field.activityId = "search";
    field.item = live;
    scene.elements.push_back(field);
    Draft before{ "search", "search", "go", "vs" };
    Draft after = before;
    after.text = "vsc";
    auto change = liveChange(before, after, scene);
    expect(change && change->actionId == "typed" && change->text == "vsc" && change->providerId == "search", "an edit sends the whole text");
    expect(!liveChange(before, before, scene), "no change, nothing sent (a caret move)");
    expect(!liveChange(std::nullopt, after, scene) && !liveChange(before, std::nullopt, scene), "focusing or leaving isn't an edit");
    Draft other = after;
    other.actionId = "elsewhere";
    expect(!liveChange(before, other, scene), "another field isn't this one's edit");
    Scene plain;
    field.item = TextInput{ .actionId = "go" };
    plain.elements.push_back(field);
    expect(!liveChange(before, after, plain), "a field that isn't live sends nothing (it waits for Enter)");
}

} // namespace

void runSearchTests()
{
    cleanupTests();
    settingsTests();
    rankingTests();
    providerTests();
    liveFieldTests();
}

} // namespace cup::test
