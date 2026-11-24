// Search by a file dialog (decision 181): its card picks a file for the
// dialog. Files and folders only, and nothing before something is typed;
// a result goes to the dialog instead of being opened; its own menu.

#include "expect.h"

#include <algorithm>
#include "panel_host.h"
#include "providers/search/result_menu.h"
#include "providers/search/search_provider.h"

namespace cup::test {
namespace {

struct PickingHost : PanelHost {
    std::vector<std::function<void(ServiceReply)>> answers;
    std::vector<std::string> closed;
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
    std::function<void(std::vector<Launchable>)> appsDone;
    std::vector<std::string> opened, given, copied;
    bool dialogTakes = true;
    SearchProvider search({ .loadApps = [&](std::function<void(std::vector<Launchable>)> d) { appsDone = std::move(d); },
                            .launch = [](const Launchable&) { return true; },
                            .openFile = [&](const std::string& p) { opened.push_back(p); return true; },
                            .exists = [](const std::string& p) { return p != "C:\\Gone\\old.txt"; },
                            .copyText = [&](const std::string& t) { copied.push_back(t); return true; },
                            .giveToDialog = [&](const std::string& p) { given.push_back(p); return dialogTakes; },
                            .windowsBuild = 22621 });
    LaunchHistory before;
    before.record("C:\\Docs\\notes.txt", host.clock - Ms{ 60000 });
    before.record("C:\\Gone\\old.txt", host.clock - Ms{ 30000 });
    before.record("C:\\Pictures\\", host.clock - Ms{ 90000 });
    host.store[SearchProvider::kOpenedKey] = before.save();
    search.start(host);
    appsDone({ { Launchable::Kind::App, "Report Viewer", "viewer", {}, nullptr } });
    auto act = [&](const std::string& id, const std::string& text = "") { search.onAction({ "search", "search", id, 0, text }); };
    auto type = [&](const std::string& text) { act(search_action::kTyped, text); };
    auto field = [&] { return std::get<TextInput>(host.published.back().content.expandedCenter[0].items[0]); };

    expect(host.published.back().picksFiles, "Search's card is the one a file dialog's pill opens");
    expect(search.results().empty() && field().placeholder == "Search apps, settings and files", "no dialog: the search it always was");

    // A dialog comes in front, before anything is typed: what was opened lately.
    search.setPicking(true);
    expect(search.results().empty() && host.published.back().content.expandedCenter.size() == 1,
           "nothing typed: the field alone, no list of what was opened before");
    expect(field().placeholder == "Find a file for this dialog", "the field says what it's for");

    // Typing: files only.
    type("report");
    expect(search.results().empty(), "no apps, commands or Settings pages for a file dialog");
    host.answers.back()({ ServiceReply::Problem::None, kFiles });
    auto r = search.results();
    expect(r.size() == 2 && r[0].file && r[1].folder, "files and folders, as the service found them");

    // Its menu.
    const Row& fileRow = host.published.back().content.expandedCenter[1];
    expect(labels(fileRow.menu) == std::vector<std::string>{ "Choose", "Go to its folder", "Copy path" }, "a file's picking menu");
    expect(fileRow.menu[0].bold && fileRow.menu[0].actionId == "search.open.0" && fileRow.menu[1].actionId == "search.showFolder.0",
           "Choose is the row's own action; the folder has its own");
    expect(labels(host.published.back().content.expandedCenter[2].menu) == std::vector<std::string>{ "Open folder", "Go to its folder", "Copy path" },
           "a folder's: the dialog opens it");
    SearchProvider::Result remembered{ .file = true, .name = "notes.txt", .target = "C:\\Docs\\notes.txt", .remembered = true };
    expect(labels(pickingMenu(remembered, 0)).back() == "Remove from history", "a remembered one can be forgotten there too");

    // Go to its folder: the dialog gets the folder, and the card closes.
    act(search_action::kShowFolder + std::string("0"));
    expect(given == std::vector<std::string>{ "C:\\Docs" } && host.closed.size() == 1, "Go to its folder gives the dialog the folder");
    act(search_action::kCopyPath + std::string("0"));
    expect(copied == std::vector<std::string>{ "C:\\Docs\\report.pdf" } && host.closed.size() == 1, "Copy path copies, and the card stays");
    act(search_action::kProperties + std::string("0"));
    act(search_action::kAdmin + std::string("0"));
    expect(given.size() == 1 && host.closed.size() == 1, "the search menu's other verbs do nothing while picking");

    // A dialog that doesn't take it (an elevated app's): the path on the clipboard, and an alert saying so.
    dialogTakes = false;
    act(search_action::kOpen + std::string("0"));
    expect(given.back() == "C:\\Docs\\report.pdf" && opened.empty() && copied.back() == "C:\\Docs\\report.pdf", "refused: its path is copied instead");
    expect(host.alerts.size() == 1 && host.alerts[0].content.label == "Path copied" && host.alerts[0].activityId.empty() && host.closed.size() == 2,
           "...an alert says to paste it, and the card closes");
    act(search_action::kShowFolder + std::string("0"));
    expect(copied.back() == "C:\\Docs" && host.alerts.size() == 2 && host.closed.size() == 3, "its folder likewise");

    // Choose: given to the dialog, not opened; remembered like a file opened from search.
    dialogTakes = true;
    act(search_action::kGo, "report");
    expect(given.back() == "C:\\Docs\\report.pdf" && opened.empty() && host.closed.size() == 4 && host.alerts.size() == 2, "Enter gives the top file to the dialog and closes the card");
    expect(LaunchHistory::load(host.store[SearchProvider::kOpenedKey]).uses.count("C:\\Docs\\report.pdf") == 1, "...and it's a recent file from then on");

    // The dialog goes: search again.
    search.setPicking(false);
    type("report");
    r = search.results();
    expect(!r.empty() && r[0].name == "Report Viewer" && !r[0].file, "the dialog gone: apps are back");
    host.answers.back()({ ServiceReply::Problem::None, kFiles });
    expect(labels(host.published.back().content.expandedCenter[2].menu).front() == "Open", "...and so is the search menu");
    act(search_action::kOpen + std::string("1"));
    expect(opened == std::vector<std::string>{ "C:\\Docs\\report.pdf" }, "...and a file is opened again");
}

} // namespace cup::test
