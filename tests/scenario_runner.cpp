// Runs *.scenario files: plain-text scripts of events and the mode the island
// must be in afterwards. Format, one step per line:
//
//   scenario: <name>                  starts a fresh island at t = 0
//   <command> [args] [-> <Mode> [key=value ...]]
//
// Commands:  start <id> [level] [relevance] [expires=<ms>]   update <id>   end <id>
//            alert <name> [level] [for=<activity>] [key=<coalesceKey>]
//            click  outside  esc  pointer  drag-enter  drag-rejected
//            drag-leave  drop  hide  show  wait <ms>
//            bubble  promote <id>  switch <id>  drag <dx> <dy>  release  cancel
//            panel <id> [static]  unpanel <id>  home  tile <id>     (static: a widget with no card)
//            focus <card> <action>  type <text...>  erase  submit  cancel-input   (a TextInput, decision 016)
//            ask <card>  close-dialog                                               (a provider's dialog, decision 020)
// Checks after "->":  <Mode>  queue=<n>  top=<activity>  bubble=<activity>  alert=<name>
//                     expanded=<activity|panel|home>  pinned=<activity>  dragging=yes|no  preview=yes|no
//                     draft=<text>|empty|none  (no spaces in <text>)  dialog=<card>|none
// Levels: passive active timeSensitive critical.  '#' starts a comment.

#include "core/island.h"
#include "expect.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace cup::test {
namespace {

std::vector<std::string> split(const std::string& text)
{
    std::istringstream in(text);
    std::vector<std::string> words;
    for (std::string w; in >> w;) words.push_back(w);
    return words;
}

std::optional<Level> parseLevel(const std::string& word)
{
    for (Level l : { Level::Passive, Level::Active, Level::TimeSensitive, Level::Critical })
        if (word == toString(l)) return l;
    return std::nullopt;
}

// "key=value" -> value if the key matches.
std::optional<std::string> option(const std::string& word, const std::string& key)
{
    if (word.rfind(key + "=", 0) != 0) return std::nullopt;
    return word.substr(key.size() + 1);
}

struct Runner {
    std::string where; // "file:line" of the current step, for messages
    IslandState state;
    Ms now{ 0 };

    void fail(const std::string& message) { expect(false, where + ": " + message); }

    void apply(const Event& e) { state = reduce(state, e, now); }

    // Advance time, firing Tick at each deadline on the way, like the app's timer.
    void wait(Ms duration)
    {
        Ms target = now + duration;
        for (int guard = 0; guard < 1000; ++guard) {
            auto deadline = nextDeadline(state);
            if (!deadline || *deadline > target) break;
            now = std::max(now, *deadline);
            apply(Tick{});
        }
        now = target;
    }

    void run(const std::vector<std::string>& w)
    {
        const std::string& cmd = w[0];
        auto arg = [&](size_t i) { return i < w.size() ? w[i] : std::string(); };

        if (cmd == "start") {
            Activity a{ .id = arg(1), .content = { .label = arg(1) } };
            for (size_t i = 2; i < w.size(); ++i) {
                if (auto l = parseLevel(w[i])) a.level = *l;
                else if (auto v = option(w[i], "expires")) a.expiresAt = now + Ms{ std::stoi(*v) };
                else a.relevance = std::stof(w[i]);
            }
            apply(ActivityStarted{ a });
        } else if (cmd == "update") {
            apply(ActivityUpdated{ arg(1), { .label = arg(1) + " (updated)" } });
        } else if (cmd == "end") {
            apply(ActivityEnded{ arg(1) });
        } else if (cmd == "alert") {
            Alert a{ .content = { .label = arg(1) } };
            for (size_t i = 2; i < w.size(); ++i) {
                if (auto l = parseLevel(w[i])) a.level = *l;
                else if (auto v = option(w[i], "for")) a.activityId = *v;
                else if (auto v = option(w[i], "key")) a.coalesceKey = *v;
                else fail("unknown alert option '" + w[i] + "'");
            }
            apply(AlertPosted{ a });
        } else if (cmd == "click") apply(PillClicked{});
        else if (cmd == "outside") apply(ClickedOutside{});
        else if (cmd == "esc") apply(EscapePressed{});
        else if (cmd == "pointer") apply(PointerMoved{});
        else if (cmd == "drag-enter") apply(DragEntered{ true });
        else if (cmd == "drag-rejected") apply(DragEntered{ false });
        else if (cmd == "drag-leave") apply(DragLeft{});
        else if (cmd == "drop") apply(Dropped{});
        else if (cmd == "hide") apply(HiddenChanged{ true });
        else if (cmd == "show") apply(HiddenChanged{ false });
        else if (cmd == "wait") wait(Ms{ std::stoi(arg(1)) });
        else if (cmd == "bubble") apply(BubbleClicked{});
        else if (cmd == "promote") apply(ActivityPromoted{ arg(1) });
        else if (cmd == "switch") apply(ActivitySwitched{ arg(1) });
        else if (cmd == "drag") apply(BubbleDragged{ { std::stof(arg(1)), std::stof(arg(2)) } });
        else if (cmd == "release") apply(BubbleReleased{});
        else if (cmd == "cancel") apply(BubbleDragCancelled{});
        else if (cmd == "panel") {
            Panel p{ .id = arg(1), .content = { .label = arg(1) } };
            if (arg(2) != "static") p.content.expandedCenter = { row({ Text{ arg(1) } }) };
            apply(PanelPublished{ p });
        }
        else if (cmd == "unpanel") apply(PanelRemoved{ arg(1) });
        else if (cmd == "home") apply(HomeOpened{});
        else if (cmd == "tile") apply(TileClicked{ arg(1) });
        else if (cmd == "focus") apply(InputFocused{ "p", arg(1), arg(2) });
        else if (cmd == "type") {
            std::string text;
            for (size_t i = 1; i < w.size(); ++i) text += (i > 1 ? " " : "") + w[i];
            apply(TextTyped{ text });
        }
        else if (cmd == "erase") apply(TextErased{});
        else if (cmd == "submit") apply(InputSubmitted{});
        else if (cmd == "cancel-input") apply(InputCancelled{});
        else if (cmd == "ask") apply(DialogShown{ { .providerId = "p", .activityId = arg(1), .title = "Sure?", .confirmAction = "yes" } });
        else if (cmd == "close-dialog") apply(DialogClosed{});
        else fail("unknown command '" + cmd + "'");
    }

    void check(const std::vector<std::string>& w)
    {
        for (const auto& word : w) {
            if (auto v = option(word, "queue")) {
                if (state.alertQueue.size() != std::stoul(*v))
                    fail("queue=" + *v + ", got " + std::to_string(state.alertQueue.size()));
            } else if (auto v = option(word, "top")) {
                std::string top = state.activities.empty() ? "none" : state.activities.front().id;
                if (top != *v) fail("top=" + *v + ", got " + top);
            } else if (auto v = option(word, "bubble")) {
                std::string second = state.activities.size() < 2 ? "none" : state.activities[1].id;
                if (second != *v) fail("bubble=" + *v + ", got " + second);
            } else if (auto v = option(word, "pinned")) {
                std::string id = state.pinnedId.empty() ? "none" : state.pinnedId;
                if (id != *v) fail("pinned=" + *v + ", got " + id);
            } else if (auto v = option(word, "dragging")) {
                std::string got = state.bubbleDrag ? "yes" : "no";
                if (got != *v) fail("dragging=" + *v + ", got " + got);
            } else if (auto v = option(word, "preview")) {
                std::string got = swapPreview(state) ? "yes" : "no";
                if (got != *v) fail("preview=" + *v + ", got " + got);
            } else if (auto v = option(word, "alert")) {
                std::string shown = state.alert ? state.alert->content.label : "none";
                if (shown != *v) fail("alert=" + *v + ", got " + shown);
            } else if (auto v = option(word, "dialog")) {
                std::string got = state.dialog ? state.dialog->activityId : "none";
                if (got != *v) fail("dialog=" + *v + ", got " + got);
            } else if (auto v = option(word, "draft")) {
                std::string got = !state.draft ? "none" : state.draft->text.empty() ? "empty" : state.draft->text;
                if (got != *v) fail("draft=" + *v + ", got " + got);
            } else if (auto v = option(word, "expanded")) {
                std::string id = state.homeOpen ? "home" : state.expandedId.empty() ? "none" : state.expandedId;
                if (id != *v) fail("expanded=" + *v + ", got " + id);
            } else if (word != toString(mode(state))) {
                fail("expected " + word + ", got " + toString(mode(state)));
            }
        }
    }
};

} // namespace

int runScenarioFile(const std::filesystem::path& file)
{
    std::ifstream in(file);
    Runner runner;
    int scenarios = 0;
    int lineNo = 0;
    for (std::string line; std::getline(in, line);) {
        ++lineNo;
        line = line.substr(0, line.find('#'));
        runner.where = file.filename().string() + ":" + std::to_string(lineNo);

        if (line.rfind("scenario:", 0) == 0) {
            runner = Runner{};
            runner.where = file.filename().string() + ":" + std::to_string(lineNo);
            ++scenarios;
            continue;
        }
        auto arrow = line.find("->");
        auto command = split(line.substr(0, arrow));
        if (!command.empty()) runner.run(command);
        if (arrow != std::string::npos) runner.check(split(line.substr(arrow + 2)));
    }
    return scenarios;
}

} // namespace cup::test
