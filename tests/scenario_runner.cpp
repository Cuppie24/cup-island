// Runs *.scenario files: plain-text scripts of events and the mode the island
// must be in afterwards. Format, one step per line:
//
//   scenario: <name>                  starts a fresh island at t = 0
//   <command> [args] [-> <Mode> [key=value ...]]
//
// Commands:  start <id> [level] [relevance] [expires=<ms>]   update <id>   end <id>
//            alert <name> [level] [for=<activity|panel>] [key=<coalesceKey>]
//            click  outside  esc  pointer  drag-enter  drag-rejected
//            drag-leave  drop  hide  show  wait <ms>
//            bubble  promote <id>  switch <id>  drag <dx> <dy>  release  cancel
//            panel <id> [static|keep]  unpanel <id>  home  tile <id>  (static: a widget with no card;
//                                                                     keep: its card stays open on a click, decision 030)
//            focus <card> <action> [at=<n>|max=<chars>]  type <text...>  erase [word]  submit  cancel-input   (a TextInput, decision 016)
//            delete [word]  caret <left|right|word-left|word-right|start|end> [select]
//            place <n> [select]  select-word <n>  select-all                     (caret and selection, decision 043)
//            open-activity                                                           (the activity shortcut, decision 050)
//            pin                                                                     (Expanded's pin, decision 046)
//            tabrow top|bottom                                                       (Settings' Tab row, decision 048)
//            ask <card>  close-dialog                                               (a provider's dialog, decision 020)
//            edit <card> <action> <text> [at=<n>]                                   (a Text edited in place, decision 029)
//            wheel <card> <list> <from> <delta> <max>  reveal <card> <list> <row>    (lists, decision 029)
//            pick <card> <list> <row> <y>  carry <y>  put-down
//            screen <width>  hold  move <dx>  let-go  restore <dock> [fraction]      (lifting the island, decision 026)
// Checks after "->":  <Mode>  queue=<n>  top=<activity>  bubble=<activity>  alert=<name>
//                     expanded=<activity|panel|home>  pinned=<activity>  dragging=yes|no  preview=yes|no
//                     draft=<text>|empty|none  (_ for a space)  dialog=<card>|none
//                     caret=<byte offset>  selected=<text>|none  (_ for a space)
//                     lifted=yes|no  spot=centre|left|right|free  stay=yes|no  tabs=top|bottom
//                     scroll=<card>/<list>:<offset>[+reveal]  carried=<row>|none
// Levels: passive active timeSensitive critical.  '#' starts a comment.

#include "core/island.h"
#include "core/lift.h"
#include "core/text_edit.h"
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

// Spaces as '_', so a checked text stays one word.
std::string underscored(std::string text)
{
    std::replace(text.begin(), text.end(), ' ', '_');
    return text;
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
        // "at=<n>" as word i: where a click put the caret.
        auto at = [&](size_t i) -> std::optional<std::size_t> {
            auto v = option(arg(i), "at");
            return v ? std::optional<std::size_t>(std::stoul(*v)) : std::nullopt;
        };

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
            p.content.keepOpen = arg(2) == "keep";
            apply(PanelPublished{ p });
        }
        else if (cmd == "unpanel") apply(PanelRemoved{ arg(1) });
        else if (cmd == "home") apply(HomeOpened{});
        else if (cmd == "open-activity") apply(ActivityOpened{});
        else if (cmd == "tile") apply(TileClicked{ arg(1) });
        else if (cmd == "focus") {
            auto max = option(arg(3), "max");
            apply(InputFocused{ "p", arg(1), arg(2), "", false, max ? std::nullopt : at(3), max ? std::stoul(*max) : 0 });
        }
        else if (cmd == "type") {
            std::string text;
            for (size_t i = 1; i < w.size(); ++i) text += (i > 1 ? " " : "") + w[i];
            apply(TextTyped{ text });
        }
        else if (cmd == "erase") apply(TextErased{ false, arg(1) == "word" });
        else if (cmd == "delete") apply(TextErased{ true, arg(1) == "word" });
        else if (cmd == "caret") {
            const char* names[] = { "left", "right", "word-left", "word-right", "start", "end" };
            auto name = std::find(std::begin(names), std::end(names), arg(1));
            if (name == std::end(names)) fail("unknown caret move '" + arg(1) + "'");
            else apply(CaretMoved{ static_cast<CaretMove>(name - std::begin(names)), arg(2) == "select" });
        }
        else if (cmd == "place") apply(CaretPlaced{ std::stoul(arg(1)), arg(2) == "select" });
        else if (cmd == "select-word") apply(WordSelected{ std::stoul(arg(1)) });
        else if (cmd == "select-all") apply(AllSelected{});
        else if (cmd == "submit") apply(InputSubmitted{});
        else if (cmd == "cancel-input") apply(InputCancelled{});
        else if (cmd == "ask") apply(DialogShown{ { .providerId = "p", .activityId = arg(1), .title = "Sure?", .confirmAction = "yes" } });
        else if (cmd == "close-dialog") apply(DialogClosed{});
        else if (cmd == "pin") apply(StayOpenToggled{});
        else if (cmd == "tabrow") apply(TabRowPlaced{ arg(1) != "bottom" });
        else if (cmd == "edit") apply(InputFocused{ "p", arg(1), arg(2), arg(3), /* editing */ true, at(4) });
        else if (cmd == "wheel") apply(ListScrolled{ arg(1), arg(2), std::stof(arg(3)), std::stof(arg(4)), std::stof(arg(5)) });
        else if (cmd == "reveal") apply(ListRevealed{ arg(1), arg(2), std::stoul(arg(3)) });
        else if (cmd == "pick") apply(ListDragStarted{ arg(1), arg(2), std::stoul(arg(3)), std::stof(arg(4)) });
        else if (cmd == "carry") apply(ListDragMoved{ std::stof(arg(1)) });
        else if (cmd == "put-down") apply(ListDragEnded{});
        else if (cmd == "screen") apply(ScreenResized{ std::stof(arg(1)) });
        else if (cmd == "hold") apply(IslandPressed{});
        else if (cmd == "move") apply(IslandDragged{ { std::stof(arg(1)), 0 } });
        else if (cmd == "let-go") apply(IslandReleased{});
        else if (cmd == "restore") {
            Spot spot{ arg(1) == "left" ? Dock::Left : arg(1) == "right" ? Dock::Right : arg(1) == "free" ? Dock::Free : Dock::Centre };
            if (spot.dock == Dock::Free) spot.fraction = std::stof(arg(2));
            apply(SpotRestored{ spot });
        }
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
                std::string got = !state.draft ? "none" : state.draft->text.empty() ? "empty" : underscored(state.draft->text);
                if (got != *v) fail("draft=" + *v + ", got " + got);
            } else if (auto v = option(word, "caret")) {
                std::string got = state.draft ? std::to_string(state.draft->caret) : "none";
                if (got != *v) fail("caret=" + *v + ", got " + got);
            } else if (auto v = option(word, "selected")) {
                std::string text = state.draft ? selectedText(*state.draft) : "";
                std::string got = text.empty() ? "none" : underscored(text);
                if (got != *v) fail("selected=" + *v + ", got " + got);
            } else if (auto v = option(word, "lifted")) {
                std::string got = isLifted(state) ? "yes" : "no";
                if (got != *v) fail("lifted=" + *v + ", got " + got);
            } else if (auto v = option(word, "spot")) {
                const char* names[] = { "centre", "left", "right", "free" };
                std::string got = names[static_cast<int>(state.spot.dock)];
                if (got != *v) fail("spot=" + *v + ", got " + got);
            } else if (auto v = option(word, "scroll")) {
                std::string key = v->substr(0, v->find(':'));
                auto it = state.scrolls.find(key);
                std::string got = it == state.scrolls.end() ? "none"
                                                            : std::to_string(static_cast<int>(it->second.offset)) + (it->second.reveal ? "+reveal" : "");
                if (key + ":" + got != *v) fail("scroll=" + *v + ", got " + got);
            } else if (auto v = option(word, "tabs")) {
                std::string got = state.tabsOnTop ? "top" : "bottom";
                if (got != *v) fail("tabs=" + *v + ", got " + got);
            } else if (auto v = option(word, "stay")) {
                std::string got = state.stayOpen ? "yes" : "no";
                if (got != *v) fail("stay=" + *v + ", got " + got);
            } else if (auto v = option(word, "carried")) {
                std::string got = state.listDrag ? std::to_string(state.listDrag->row) : "none";
                if (got != *v) fail("carried=" + *v + ", got " + got);
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
