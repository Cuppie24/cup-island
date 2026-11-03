// Search (decisions 077, 079): what's kept from Windows' app list, how apps
// and Settings pages rank, what was launched before, the card's results as
// you type (files through the host), Enter and the card closing after; and
// the live field that sends every edit.

#include "core/card_overlays.h"
#include "core/keyboard.h"
#include "core/layout.h"

#include <cstdio>
#include "core/tokens.h"
#include "expect.h"
#include "panel_host.h"
#include "providers/search/search_provider.h"

namespace cup::test {
namespace {

using Kind = Launchable::Kind;

const Placed* placed(const Scene& scene, const std::string& key)
{
    for (const Placed& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

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
    expect(host.revealed == "results@0", "each new query shows the list from its top");
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

// The card as laid out (decision 079): the regressions seen on the first
// run were a card in the media layout, the field pushed off the top by the
// results, rows over the tab row, and names cut to "W..." by long paths.
void layoutTests()
{
    auto cardWith = [](std::size_t files, bool tabsOnTop = true) {
        SearchHost host;
        SearchProvider search({ .loadApps = [](std::function<void(std::vector<Launchable>)> d) { d({}); } });
        search.start(host);
        search.onAction({ "search", "search", search_action::kTyped, 0, "wall" });
        std::string hits;
        for (std::size_t i = 0; i < files; ++i)
            hits += std::string(i ? "," : "") + R"({"name":"wallpaper)" + std::to_string(i) +
                    R"(.jpg","path":"C:\\Users\\someone\\AppData\\Local\\Microsoft\\Windows\\Themes\\wallpaper.jpg"})";
        host.answers.back()({ ServiceReply::Problem::None, R"({"jsonrpc":"2.0","id":1,"result":{"hits":[)" + hits + "]}}" });
        Panel p = host.published.back();
        p.providerId = "search";
        IslandState s;
        s = reduce(s, TabRowPlaced{ tabsOnTop }, Ms{ 0 });
        s = reduce(s, PanelPublished{ p }, Ms{ 0 });
        s = reduce(s, PanelPublished{ Panel{ .id = "other", .content = { .label = "other", .expandedCenter = { row({ Text{ "x" } }) } } } }, Ms{ 0 });
        s = reduce(s, TileClicked{ "search" }, Ms{ 0 });
        return std::pair{ s, layout(s, Ms{ 0 }, [](const Text& t) { return 8.0f * static_cast<float>(t.value.size()); }) };
    };
    auto [few, small] = cardWith(1);
    auto [many, big] = cardWith(8);
    expect(few.draft && few.draft->actionId == search_action::kGo, "the card opens with its field taking the keys");
    expect(escapeCloses(few), "Esc closes it at once");
    expect(small.size == big.size, "one height however many results: the card doesn't jump as you type");
    const Placed* field = nullptr;
    const Placed* name = nullptr; // a file's name, and the folder after it
    const Placed* folder = nullptr;
    float listTop = big.size.y, listBottom = 0, tabsTop = big.size.y, tabsBottom = 0;
    std::size_t listed = 0;
    for (const Placed& e : big.elements) {
        if (std::holds_alternative<TextInput>(e.item)) field = &e;
        if (e.list) {
            listTop = std::min(listTop, e.list->viewport.y);
            listBottom = std::max(listBottom, e.list->viewport.y + e.list->viewport.height);
            ++listed;
        }
        if (e.key.rfind("tab/", 0) == 0) {
            tabsTop = std::min(tabsTop, e.frame.y);
            tabsBottom = std::max(tabsBottom, e.frame.y + e.frame.height);
        }
        if (auto* text = std::get_if<Text>(&e.item); text && !name && text->value.rfind("wallpaper", 0) == 0) name = &e;
        if (auto* text = std::get_if<Text>(&e.item); text && name && !folder && text->yields) folder = &e;
    }
    expect(field && field->frame.y >= tabsBottom && field->frame.y + field->frame.height <= big.size.y,
           "with many results the field stays in the card, clear of the tab row");
    expect(listed > 0 && listTop >= field->frame.y + field->frame.height && listBottom <= big.size.y,
           "the results sit under the field and inside the card: they scroll, they don't spill");
    expect(listBottom - listTop > 4 * 40, "and they fill the card's height, not a strip of it");
    expect(name && folder && name->frame.width >= 8.0f * 14 && folder->frame.width < 8.0f * 55,
           "a file's name keeps its width beside a long folder path");
    expect(field && name && std::abs(field->frame.x - name->frame.x) < 64, "rows start at the card's side, not after a media art column");

    // Keys (decision 079): the field keeps them for typing; Up and Down move
    // the highlight through the rows, as in a launcher.
    auto first = moveAmongRows(big, "", Arrow::Down);
    expect(first && first->id.rfind("el:expandedCenter/1/", 0) == 0 && !fieldStop(big, *first), "Down from the field: the first row");
    auto second = first ? moveAmongRows(big, first->id, Arrow::Down) : std::nullopt;
    expect(second && second->id.rfind("el:expandedCenter/2/", 0) == 0, "Down again: the next row");
    expect(second && moveAmongRows(big, second->id, Arrow::Up)->id == first->id, "Up: back a row");
    expect(first && !moveAmongRows(big, first->id, Arrow::Up), "Up from the first row: back to typing, nothing highlighted");
    expect(!moveAmongRows(big, "", Arrow::Up), "Up with nothing highlighted: still typing");
    std::string at = first ? first->id : "";
    for (int i = 0; i < 30 && !at.empty(); ++i) at = moveAmongRows(big, at, Arrow::Down)->id;
    expect(!at.empty() && moveAmongRows(big, at, Arrow::Down)->id == at, "Down at the last row stays there");
    expect(field && fieldStop(big, Focusable{ .id = "el:" + field->key }), "the field is a field stop");
    auto [low, tabsBelow] = cardWith(2, false);
    (void)low;
    std::string last = moveAmongRows(tabsBelow, "", Arrow::Down)->id;
    last = moveAmongRows(tabsBelow, last, Arrow::Down)->id;
    for (int i = 0; i < 10; ++i) last = moveAmongRows(tabsBelow, last, Arrow::Down)->id;
    expect(last.rfind("el:", 0) == 0, "tabs at the bottom: Down at the last row doesn't leave for the tab row");
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
    layoutTests();
    liveFieldTests();
}

} // namespace cup::test
