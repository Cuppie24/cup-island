// Search for a file dialog's own island (decision 181): an activity there
// whose card picks a file for the dialog. Files and folders only, and nothing
// before something is typed; a result goes to the dialog instead of being
// opened; its own menu; an elevated app's dialog gets the path copied.

#include "expect.h"

#include "panel_host.h"
#include "providers/search/result_menu.h"
#include "providers/search/search_provider.h"

namespace cup::test {
namespace {

struct PickingHost : PanelHost {
    std::vector<Activity> started;
    std::vector<std::function<void(ServiceReply)>> answers;
    std::vector<std::string> closed;
    void startActivity(Activity a) override { started.push_back(std::move(a)); }
    void callService(const std::string&, const std::string&, std::function<void(ServiceReply)> done) override { answers.push_back(std::move(done)); }
    void closeCard(const std::string& id) override { closed.push_back(id); }
};

std::vector<std::string> labels(const std::vector<MenuItem>& menu)
{
    std::vector<std::string> out;
    for (const MenuItem& m : menu) out.push_back(m.label);
    return out;
}

const char* kFiles = R"({"jsonrpc":"2.0","id":1,"result":{"hits":[)"
                     R"({"name":"report.pdf","path":"C:\\Docs\\report.pdf","folder":false,"hidden":false},)"
                     R"({"name":"Reports","path":"C:\\Docs\\Reports","folder":true,"hidden":false}]}})";

} // namespace

void runSearchPickingTests()
{
    PickingHost host;
    host.clock = Ms{ 100LL * 24 * 3600 * 1000 };
    std::vector<std::string> opened, given, copied;
    bool dialogTakes = true;
    SearchProvider search({ .launch = [](const Launchable&) { return true; },
                            .openFile = [&](const std::string& p) { opened.push_back(p); return true; },
                            .exists = [](const std::string&) { return true; },
                            .copyText = [&](const std::string& t) { copied.push_back(t); return true; },
                            .giveToDialog = [&](const std::string& p) { given.push_back(p); return dialogTakes; },
                            .forDialog = true,
                            .windowsBuild = 22621 });
    LaunchHistory before;
    before.record("C:\\Docs\\notes.txt", host.clock - Ms{ 60000 });
    host.store[SearchProvider::kOpenedKey] = before.save();
    search.start(host);
    auto act = [&](const std::string& id, const std::string& text = "") { search.onAction({ "search", "search", id, 0, text }); };
    auto type = [&](const std::string& text) { act(search_action::kTyped, text); };
    auto card = [&]() -> const Content& { return host.started.back().content; };

    // On the dialog's island it's an activity: a pill that says what it's for, and its card.
    expect(host.published.empty() && !host.started.empty() && host.started.back().id == "search", "an activity on the dialog's island, not a page-row button");
    expect(card().compactLeading.size() == 1 && std::holds_alternative<Icon>(card().compactLeading[0].items[0]) && card().label == "Find a file",
           "its pill: a search glyph and what it's for");
    expect(card().noTopBand && card().hidesTabs && card().fieldFirst == search_action::kGo, "its card: the field first, no tabs, no top band to click");
    expect(std::get<TextInput>(card().expandedCenter[0].items[0]).placeholder == "Find a file for this dialog", "the field says what it's for");
    expect(search.results().empty() && card().expandedCenter.size() == 1, "nothing typed: the field alone, no list of what was opened before");

    // Typing: files only.
    type("report");
    expect(search.results().empty(), "no apps, commands or Settings pages for a file dialog");
    host.answers.back()({ ServiceReply::Problem::None, kFiles });
    auto r = search.results();
    expect(r.size() == 2 && r[0].file && r[1].folder, "files and folders, as the service found them");

    // Its menu.
    const Row& fileRow = card().expandedCenter[1];
    expect(labels(fileRow.menu) == std::vector<std::string>{ "Choose", "Go to its folder", "Copy path" }, "a file's picking menu");
    expect(fileRow.menu[0].bold && fileRow.menu[0].actionId == "search.open.0" && fileRow.menu[1].actionId == "search.showFolder.0",
           "Choose is the row's own action; the folder has its own");
    expect(labels(card().expandedCenter[2].menu) == std::vector<std::string>{ "Open folder", "Go to its folder", "Copy path" }, "a folder's: the dialog opens it");
    SearchProvider::Result remembered{ .file = true, .name = "notes.txt", .target = "C:\\Docs\\notes.txt", .remembered = true };
    expect(labels(pickingMenu(remembered, 0)) == std::vector<std::string>{ "Choose", "Go to its folder", "Copy path" },
           "no Remove from history: the picking card shows no history");

    // Go to its folder: the dialog gets the folder, and the card closes.
    act(search_action::kShowFolder + std::string("0"));
    expect(given == std::vector<std::string>{ "C:\\Docs" } && host.closed.size() == 1, "Go to its folder gives the dialog the folder");
    act(search_action::kCopyPath + std::string("0"));
    expect(copied == std::vector<std::string>{ "C:\\Docs\\report.pdf" } && host.closed.size() == 1, "Copy path copies, and the card stays");
    act(search_action::kProperties + std::string("0"));
    act(search_action::kAdmin + std::string("0"));
    expect(given.size() == 1 && host.closed.size() == 1, "the search menu's other verbs do nothing while picking");

    // A dialog that doesn't take it (an elevated app's): the path on the clipboard, and an alert on this island saying so.
    dialogTakes = false;
    act(search_action::kOpen + std::string("0"));
    expect(given.back() == "C:\\Docs\\report.pdf" && opened.empty() && copied.back() == "C:\\Docs\\report.pdf", "refused: its path is copied instead");
    expect(host.alerts.size() == 1 && host.alerts[0].content.label == "Path copied" && host.alerts[0].activityId.empty() && host.closed.size() == 2,
           "...an alert says to paste it, and the card closes");
    act(search_action::kShowFolder + std::string("0"));
    expect(copied.back() == "C:\\Docs" && host.alerts.size() == 2 && host.closed.size() == 3, "its folder likewise");

    // Choose: given to the dialog, not opened.
    dialogTakes = true;
    act(search_action::kGo, "report");
    expect(given.back() == "C:\\Docs\\report.pdf" && opened.empty() && host.closed.size() == 4 && host.alerts.size() == 2,
           "Enter gives the top file to the dialog and closes the card");

    // The island's own Search is the search it always was.
    PickingHost island;
    SearchProvider own({ .openFile = [&](const std::string& p) { opened.push_back(p); return true; }, .windowsBuild = 22621 });
    own.start(island);
    expect(island.started.empty() && island.published.back().rowButton == Glyph::Search &&
               std::get<TextInput>(island.published.back().content.expandedCenter[0].items[0]).placeholder == "Search apps, settings and files" &&
               !island.published.back().content.noTopBand,
           "without forDialog: the page-row button and its card, as ever");
}

} // namespace cup::test
