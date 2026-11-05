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
#include "providers/search/recent_files.h"
#include "providers/search/result_menu.h"
#include "providers/search/result_rows.h"
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
    expect(!names(rankLaunchables(items, "", used, now, 10)).empty(), "nothing typed: what was launched before");
    expect(rankLaunchables(items, "d:/projects", used, now, 10).empty() && rankLaunchables(items, "ext:pdf", used, now, 10).empty() &&
               rankLaunchables(items, "projects/chrom", used, now, 10).empty(),
           "a path or a filter is for files: no apps, not even the ones launched before");
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
    expect(search.results().empty() && host.published.back().content.expandedCenter.size() == 1, "nothing typed: the field alone");

    // Rows (decision 079): two lines, the typed words picked out, a file's folder one click away.
    type("power");
    host.answers.back()({ ServiceReply::Problem::None,
                          R"({"jsonrpc":"2.0","id":9,"result":{"hits":[{"name":"power.txt","path":"C:\\Users\\me\\power.txt"}]}})" });
    const Slot& rows = host.published.back().content.expandedCenter;
    const Row& fileRow = rows.back();
    auto* name = std::get_if<Text>(&fileRow.items[1]);
    expect(name && name->value == "power.txt" && name->bright == std::vector<std::pair<std::size_t, std::size_t>>{ { 0, 5 } },
           "a file's name, the typed word in it bright");
    expect(name && name->detail == "C:\\Users\\me", "and its folder under it");
    auto* open = std::get_if<Button>(&fileRow.items[0]);
    expect(open && open->actionId.rfind(search_action::kOpen, 0) == 0 && fileRow.clickAnywhere, "its icon is the row's button: the whole row opens it");
    expect(fileRow.trailing.empty() && !fileRow.menu.empty(), "no folder button: its menu has Reveal (decision 086)");
    std::string revealIt;
    for (const MenuItem& item : fileRow.menu)
        if (item.label == "Reveal in Explorer") revealIt = item.actionId;
    search.onAction({ "search", "search", revealIt, 0, "" });
    expect(!revealIt.empty() && host.closed.size() == 2, "no reveal hook: nothing happens");
    search.onCardClosed("search");
    expect(search.results().empty() && host.published.back().content.expandedCenter.size() == 1, "closed: it opens empty next time");
}

// A result's menu (decision 086): what each kind offers, in order, with its
// keys, and what each item does.
std::vector<std::string> labels(const std::vector<MenuItem>& menu)
{
    std::vector<std::string> out;
    for (const MenuItem& item : menu) out.push_back(item.label);
    return out;
}

void menuTests()
{
    using R = SearchProvider::Result;
    using L = std::vector<std::string>;
    R program{ .kind = Kind::App, .name = "Chrome", .target = "shell:AppsFolder\\Chrome", .location = "C:\\Chrome\\chrome.exe" };
    R store{ .kind = Kind::App, .name = "Calculator", .target = "shell:AppsFolder\\Calc!App", .package = "Microsoft.WindowsCalculator_8wekyb3d8bbwe" };
    R setting{ .kind = Kind::Setting, .name = "Display", .target = "ms-settings:display", .remembered = true };
    R panel{ .kind = Kind::ControlPanel, .name = "Power Options", .target = "::{power}" };
    R file{ .file = true, .name = "report.pdf", .target = "C:\\Docs\\report.pdf", .remembered = true };
    R script{ .file = true, .name = "build.cmd", .target = "C:\\my.dir\\build.cmd" };
    R folder{ .file = true, .folder = true, .name = "Docs", .target = "C:\\Docs" };
    expect(labels(resultMenu(program, 0)) == L{ "Open", "Run as administrator", "Reveal in Explorer", "Open in Terminal", "Copy path", "Uninstall", "Properties" },
           "a desktop program: run elevated, revealed, its path, uninstalled, its properties");
    expect(labels(resultMenu(store, 0)) == L{ "Open", "App settings", "Uninstall" }, "a Store app: none of a program file's items");
    expect(labels(resultMenu(setting, 0)) == L{ "Open", "Copy path", "Remove from history" }, "a Settings page: its link, and out of the history");
    expect(labels(resultMenu(panel, 0)) == L{ "Open" }, "a Control Panel item: Open");
    expect(labels(resultMenu(file, 0)) == L{ "Open", "Reveal in Explorer", "Open in Terminal", "Copy", "Copy path", "Remove from history", "Properties" },
           "a file: the file itself copied, a recent one forgotten");
    expect(labels(resultMenu(script, 0))[1] == "Run as administrator", "a script runs as administrator (a dot in a folder isn't its type)");
    expect(labels(resultMenu(folder, 0)) == L{ "Open", "Reveal in Explorer", "Open in Terminal", "Copy", "Copy path", "Properties" }, "a folder");
    R noLocation = program;
    noLocation.location.clear();
    expect(labels(resultMenu(noLocation, 0)) == L{ "Open", "Run as administrator", "Uninstall" }, "a program whose file isn't known: nothing to reveal");

    std::vector<MenuItem> m = resultMenu(file, 3);
    expect(m[0].bold && m[0].actionId == "search.open.3" && m[0].chord == Chord{ .key = kEnter }, "Open first, bold, Enter, on its index");
    auto chordOf = [&](const std::string& label) {
        for (const MenuItem& item : m)
            if (item.label == label) return item.chord ? chordText(*item.chord) : std::string();
        return std::string("missing");
    };
    expect(chordOf("Reveal in Explorer") == "Ctrl+Shift+E" && chordOf("Open in Terminal") == "Ctrl+Shift+T" && chordOf("Copy") == "Ctrl+C" &&
               chordOf("Copy path") == "Ctrl+Shift+C" && chordOf("Remove from history") == "Shift+Delete" && chordOf("Properties") == "Alt+Enter",
           "each item's keys");
    expect(chordText(*resultMenu(program, 0)[1].chord) == "Ctrl+Shift+Enter", "Run as administrator: Ctrl+Shift+Enter");
    expect(m[1].group == m[2].group && m[2].group != m[3].group && m[3].group == m[4].group && m[5].group != m[6].group,
           "groups: open, places, copies, history, properties");

    // What they do, through the hooks.
    SearchHost host;
    std::vector<std::string> calls;
    auto hook = [&](const char* name) { return [&calls, name](const std::string& arg) { calls.push_back(std::string(name) + " " + arg); return true; }; };
    SearchProvider search({ .loadApps = [&](std::function<void(std::vector<Launchable>)> d) {
                               Launchable chrome{ .name = "Chrome", .target = "shell:AppsFolder\\Chrome", .location = "C:\\Chrome\\chrome.exe" };
                               Launchable calc{ .name = "Calculator", .target = "Calc!App", .package = "Microsoft.Calc_8wekyb3d8bbwe" };
                               d({ chrome, calc });
                           },
                            .launch = [&](const Launchable& l) { calls.push_back("launch " + l.target); return true; },
                            .reveal = hook("reveal"),
                            .runAsAdmin = hook("admin"),
                            .openTerminal = hook("terminal"),
                            .properties = hook("properties"),
                            .copyText = hook("copyText"),
                            .copyFile = hook("copyFile") });
    search.start(host);
    auto type = [&](const std::string& text) { search.onAction({ "search", "search", search_action::kTyped, 0, text }); };
    auto act = [&](std::string id) { search.onAction({ "search", "search", std::move(id), 0, "" }); };
    type("chrome");
    host.answers.back()({ ServiceReply::Problem::None,
                          R"({"jsonrpc":"2.0","id":1,"result":{"hits":[{"name":"chrome.zip","path":"C:\\Downloads\\chrome.zip"}]}})" });
    expect(search.results().size() == 2, "the program, then a file");
    act("search.admin.0");
    act("search.reveal.0");
    act("search.terminal.0");
    act("search.properties.0");
    act("search.copyPath.0");
    act("search.copy.1");
    act("search.terminal.1");
    act("search.uninstall.0");
    expect(calls == L{ "admin shell:AppsFolder\\Chrome", "reveal C:\\Chrome\\chrome.exe", "terminal C:\\Chrome", "properties C:\\Chrome\\chrome.exe",
                       "copyText C:\\Chrome\\chrome.exe", "copyFile C:\\Downloads\\chrome.zip", "terminal C:\\Downloads", "launch ms-settings:appsfeatures" },
           "each item calls its hook: elevated on the shell item, the rest on the program's file");
    expect(host.closed.size() == 6, "what opens a window closes the card; copying doesn't");
    calls.clear();
    act("search.copy.0");
    act("search.admin.1");
    expect(calls.empty(), "a program can't be copied, a zip can't be elevated");
    type("calc");
    act("search.appSettings.0");
    act("search.reveal.0");
    expect(calls == L{ "launch ms-settings:appsfeatures-app?Microsoft.Calc_8wekyb3d8bbwe" }, "a Store app's own settings page; nothing to reveal");

    // Remove from history: launched, it ranks first and offers it; removed, it doesn't.
    calls.clear();
    act("search.open.0");
    expect(search.results()[0].remembered, "launched: in the history");
    L offered = labels(resultMenu(search.results()[0], 0));
    expect(std::find(offered.begin(), offered.end(), "Remove from history") != offered.end(), "and its menu offers to remove it");
    act("search.forget.0");
    expect(!search.results()[0].remembered && host.store[SearchProvider::kHistoryKey].find("Calc!App") == std::string::npos,
           "removed, and saved");
}

void rowTextTests()
{
    using Parts = std::vector<std::pair<std::size_t, std::size_t>>;
    expect(matchedParts("q3-report.pdf", "rep") == Parts{ { 3, 3 } }, "a word lit where it starts a word in the name");
    expect(matchedParts("prepare report", "rep") == Parts{ { 8, 3 } }, "a word start beats an earlier match inside a word");
    expect(matchedParts("prepare", "rep") == Parts{ { 1, 3 } }, "else the first match anywhere");
    expect(matchedParts("Report Q3", "q3 REP") == Parts{ { 0, 3 }, { 7, 2 } }, "every word, in the name's order, any case");
    expect(matchedParts("report", "re rep") == Parts{ { 0, 3 } }, "overlapping words light one part");
    expect(matchedParts("Отчёт.docx", "отч") == Parts{ { 0, 6 } }, "Russian, in bytes");
    expect(matchedParts("cwatis", "projects/cwa") == Parts{ { 0, 3 } }, "a path word by its last part");
    expect(matchedParts("notes", "zz ext:txt").empty(), "no word in it: nothing lit");
    expect(shortPath("C:\\Users\\Said\\Documents", "C:\\Users\\Said") == "~\\Documents", "the user's folder is ~");
    expect(shortPath("c:\\users\\said", "C:\\Users\\Said") == "~", "in any case");
    expect(shortPath("C:\\Users\\Said2\\x", "C:\\Users\\Said") == "C:\\Users\\Said2\\x", "a neighbour's folder isn't inside");
    expect(shortPath("D:\\x", "") == "D:\\x", "no home known: as it is");
    expect(folderOf("C:\\a\\b.txt") == "C:\\a" && folderOf("C:\\a") == "C:\\", "a file's folder; a drive's top level keeps its slash");
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
    auto [none, bare] = cardWith(0);
    auto [few, small] = cardWith(1);
    auto [many, big] = cardWith(8);
    auto [more, biggest] = cardWith(20);
    expect(few.draft && few.draft->actionId == search_action::kGo, "the card opens with its field taking the keys");
    expect(escapeCloses(few), "Esc closes it at once");
    expect(bare.size.y < small.size.y && small.size.y < big.size.y, "as tall as its rows: it grows with the results");
    expect(biggest.size.y <= big.size.y + 1 && big.size.y - small.size.y > 3 * tokens().smallButtonSize,
           "up to searchVisibleRows of them, then they scroll");
    expect(bare.size.x == big.size.x && big.size.x == tokens().searchCardWidth, "searchCardWidth wide, typed or not");
    const Placed* field = nullptr;
    const Placed* name = nullptr; // a file's name
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
    }
    expect(!big.tabs && tabsBottom == 0, "no tab row over it, though other cards are open (Content::hidesTabs)");
    expect(field && field->frame.y >= tabsBottom && field->frame.y + field->frame.height <= big.size.y,
           "with many results the field stays in the card, clear of the tab row");
    expect(listed > 0 && listTop >= field->frame.y + field->frame.height && listBottom <= big.size.y,
           "the results sit under the field and inside the card: they scroll, they don't spill");
    expect(field && listTop - (field->frame.y + field->frame.height) >= tokens().groupGap,
           "room between the field and the first result, which is lit: not stuck together");
    expect(listBottom - listTop > 4 * 40, "and they fill the card's height, not a strip of it");
    expect(name && name->frame.width > big.size.x / 2, "a file's name has the row's width: its path is under it, not beside it");
    expect(field && name && std::abs(field->frame.x - name->frame.x) < 64, "rows start at the card's side, not after a media art column");

    // Keys (decision 079): the field keeps them for typing; Up and Down move
    // the highlight through the rows, as in a launcher.
    auto first = moveAmongRows(big, "", Arrow::Down);
    expect(first && first->id.rfind("el:expandedCenter/1/", 0) == 0 && !fieldStop(big, *first), "Down from the field: the first row");
    auto second = first ? moveAmongRows(big, first->id, Arrow::Down) : std::nullopt;
    expect(second && second->id.rfind("el:expandedCenter/2/", 0) == 0, "Down again: the next row");
    expect(second && moveAmongRows(big, second->id, Arrow::Up)->id == first->id, "Up: back a row");
    expect(first && !moveAmongRows(big, first->id, Arrow::Up), "Up from the first row: back to typing, nothing highlighted");
    expect(first && defaultRow(big) && defaultRow(big)->id == first->id, "typing, the first row is the one lit: Enter opens it");
    const Placed* firstButton = nullptr;
    for (const Placed& e : big.elements)
        if (first && "el:" + e.key == first->id) firstButton = &e;
    expect(first && firstButton && firstButton->listRow && first->frame.height == firstButton->listRow->height &&
               first->frame.height > firstButton->frame.height,
           "the keys light the whole row, as the pointer does, not just its icon");
    expect(firstButton && firstButton->listRow->height >= 2 * tokens().listItemPadding + tokens().buttonSize,
           "a result row is as tall as a full-size icon button");
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

// Recently opened files (decision 082): ranked first within their match
// class, never above a better match; shown even if the service left them
// out; forgotten once gone.
void recentTests()
{
    using Row = indexer::ResultRow;
    constexpr Ms kDay{ 24 * 60 * 60 * 1000 };
    Ms now = 100 * kDay;
    auto file = [](std::string path) {
        std::size_t slash = path.find_last_of('\\');
        return Row{ .name = path.substr(slash + 1), .path = path };
    };
    auto names = [](const std::vector<Row>& rows) {
        std::string out;
        for (const Row& r : rows) out += (out.empty() ? "" : ",") + r.name;
        return out;
    };
    auto everywhere = [](const std::string&) { return true; };
    std::vector<std::string> gone;

    std::vector<Row> service = { file("C:\\report.pdf"), file("C:\\a\\report-old.pdf"), file("C:\\a\\q3-report.pdf"), file("C:\\b\\q4-report.pdf") };
    LaunchHistory opened;
    opened.record("C:\\b\\q4-report.pdf", now - kDay);
    auto rows = withRecent(service, opened, "report", now, 10, everywhere, gone);
    expect(names(rows) == "report.pdf,report-old.pdf,q4-report.pdf,q3-report.pdf",
           "a recent file moves up within its match class, not past a better match");
    opened.record("C:\\a\\report-old.pdf", now - 20 * kDay);
    opened.record("C:\\report.pdf", now - 2 * kDay);
    rows = withRecent(service, opened, "report", now, 10, everywhere, gone);
    expect(names(rows) == "report.pdf,report-old.pdf,q4-report.pdf,q3-report.pdf", "an exact match stays first");
    LaunchHistory both;
    both.record("C:\\a\\q3-report.pdf", now - 25 * kDay);
    both.record("C:\\b\\q4-report.pdf", now - 3 * kDay);
    expect(names(withRecent(service, both, "report", now, 10, everywhere, gone)) == "report.pdf,report-old.pdf,q4-report.pdf,q3-report.pdf",
           "the more recent first");
    LaunchHistory old;
    old.record("C:\\b\\q4-report.pdf", now - 40 * kDay);
    expect(names(withRecent(service, old, "report", now, 10, everywhere, gone)) == names(service), "older than a month: the service's order");

    LaunchHistory missing;
    missing.record("D:\\notes\\report-draft.txt", now - kDay);
    missing.record("D:\\photos\\", now - kDay); // a folder: doesn't match
    rows = withRecent(service, missing, "report", now, 10, everywhere, gone);
    expect(rows.size() == 5 && rows[1].name == "report-draft.txt" && rows[1].path == "D:\\notes\\report-draft.txt",
           "a recent file the service left out still shows, first among its equals");
    expect(withRecent(service, missing, "report", now, 4, everywhere, gone).size() == 4, "within the limit");
    rows = withRecent(service, missing, "report ext:pdf", now, 10, everywhere, gone);
    expect(rows.size() == 4, "with a filter, only what the service sent");
    gone.clear();
    rows = withRecent(service, missing, "report", now, 10, [](const std::string&) { return false; }, gone);
    expect(rows.size() == 4 && gone == std::vector<std::string>{ "D:\\notes\\report-draft.txt" }, "gone from disk: not shown, and to forget");
    expect(withRecent({}, missing, "draft", now, 10, everywhere, gone).size() == 1, "even with no service at all");

    Row folder{ .name = "Projects", .path = "D:\\Projects", .folder = true };
    expect(recentKey(folder) == "D:\\Projects\\" && rowOfKey(recentKey(folder)) == folder, "a folder keeps being a folder");
    expect(rowOfKey("C:\\x\\a.txt") == file("C:\\x\\a.txt"), "a file's name and path back");

    // The provider: what's opened is remembered, and ranks higher next time.
    SearchHost host;
    std::vector<std::string> opens;
    SearchProvider search({ .loadApps = [](std::function<void(std::vector<Launchable>)> d) { d({}); },
                            .openFile = [&](const std::string& p) { opens.push_back(p); return true; },
                            .exists = everywhere });
    search.start(host);
    auto answer = [&](const char* typed) {
        search.onAction({ "search", "search", search_action::kTyped, 0, typed });
        host.answers.back()({ ServiceReply::Problem::None,
                              R"({"jsonrpc":"2.0","id":1,"result":{"hits":[{"name":"plan.txt","path":"C:\\plan.txt"},{"name":"plan-b.txt","path":"C:\\x\\plan-b.txt"},{"name":"plan-c.txt","path":"C:\\x\\plan-c.txt"}]}})" });
    };
    answer("plan");
    std::size_t c = 0;
    for (const auto& r : search.results()) {
        if (r.name == "plan-c.txt") break;
        ++c;
    }
    search.onAction({ "search", "search", search_action::kOpen + std::to_string(c), 0, "" });
    expect(opens == std::vector<std::string>{ "C:\\x\\plan-c.txt" } && host.store[SearchProvider::kOpenedKey].find("plan-c.txt") != std::string::npos,
           "an opened file is remembered");
    answer("plan");
    std::vector<std::string> order;
    for (const auto& r : search.results())
        if (r.file) order.push_back(r.name);
    expect(order == std::vector<std::string>{ "plan.txt", "plan-c.txt", "plan-b.txt" }, "and next time it's first among its equals");
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
    menuTests();
    cleanupTests();
    settingsTests();
    rankingTests();
    providerTests();
    layoutTests();
    rowTextTests();
    recentTests();
    liveFieldTests();
}

} // namespace cup::test
