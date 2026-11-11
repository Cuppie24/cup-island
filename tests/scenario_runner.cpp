// Runs *.scenario files: plain-text scripts of events and the mode the island
// must be in afterwards. Format, one step per line:
//
//   scenario: <name>                  starts a fresh island at t = 0
//   <command> [args] [-> <Mode> [key=value ...]]
//
// Commands:  start <id> [level] [relevance] [expires=<ms>] [by=<provider>]   update <id>   end <id>
//            alert <name> [level] [for=<activity|panel>] [key=<coalesceKey>] [refresh]
//            click  outside  esc  pointer  drag-enter  drag-rejected
//            drag-leave  drop  hide  show  wait <ms>
//            bubble  promote <id>  switch <id>  drag <dx> <dy>  release  cancel
//            panel <id> [static|keep|awake|field|menu|menu=changed]  unpanel <id>  home  tile <id>  (static: a widget with no card;
//                                                                     field: a card that is its field "go", decision 079;
//                                                                     menu: field, then rows 1 and 2 with menus Open, Run (dim), Reveal;
//                                                                     menu=changed: the same rows, other items;
//                                                                     keep: its card stays open on a click, decision 030;
//                                                                     awake: keep, and no idle timeout, decision 062)
//            focus <card> <action> [at=<n>|max=<chars>]  type <text...>  erase [word]  submit  cancel-input   (a TextInput, decision 016)
//            delete [word]  caret <left|right|word-left|word-right|start|end> [select]
//            place <n> [select]  select-word <n>  select-all                     (caret and selection, decision 043)
//            open-activity                                                           (the activity shortcut, decision 050)
//            close-card <id>                                                         (its provider closes it, decision 079)
//            pin                                                                     (Expanded's pin, decision 046)
//            acted <provider>  open-window <provider> <id> [page=<page>]  update-window <provider> <id> [pages=<a,b>]
//            close-window <provider> <id>  pick-page <provider> <id> <page>  stop-provider <provider>
//                                                                    (providers' windows, decision 090: open-window's pages are
//                                                                     a, b, c; update-window's are pages=, default a)
//            tabrow top|bottom                                                       (Settings' Tab row, decision 048)
//            ask <card>  close-dialog                                               (a provider's dialog, decision 020)
//            toast <card>  toast-acted                                              (a provider's toast, decision 071)
//            menu <card> <row> [pointer]  menu-step <1|-1>  menu-end first|last      (a row's menu, decision 086: by the keys, or a right-click)
//            menu-letter <c>  menu-light <n>|none  menu-close
//            edit <card> <action> <text> [at=<n>]                                   (a Text edited in place, decision 029)
//            wheel <card> <list> <from> <delta> <max>  reveal <card> <list> <row>    (lists, decision 029)
//            pick <card> <list> <row> <y>  carry <y>  put-down
//            screen <width>  hold  move <dx>  let-go  restore <dock> [fraction]      (lifting the island, decision 026)
// Checks after "->":  <Mode>  queue=<n>  top=<activity>  bubble=<activity>  alert=<name>
//                     expanded=<activity|panel|home>  pinned=<activity>  dragging=yes|no  preview=yes|no
//                     draft=<text>|empty|none  (_ for a space)  dialog=<card>|none
//                     caret=<byte offset>  selected=<text>|none  (_ for a space)
//                     lifted=yes|no  spot=centre|left|right|free  stay=yes|no  tabs=top|bottom
//                     scroll=<card>/<list>:<offset>[+reveal]  carried=<row>|none  squash=yes|no (a press on empty space, decision 071)
//                     toast=<card>|none
//                     menu=<card>/<row>|none  lit=<item>|none   (the island's open menu, and its lit item)
//                     windows=<provider/id,...>|none  front=<provider/id>|none  page=<provider/id>:<page>  opened=<result>
//                     (opened: what the last open-window got: opened, raised, notnow, toomany, badid)
// Levels: passive active timeSensitive critical.  '#' starts a comment.

#include "core/island.h"
#include "core/open_windows.h"
#include "core/lift.h"
#include "core/menu.h"
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

// "a,b", or "none" for nothing.
std::string joined(const std::vector<std::string>& ids)
{
    std::string out;
    for (const auto& id : ids) out += (out.empty() ? "" : ",") + id;
    return out.empty() ? "none" : out;
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
    Windows windows; // beside the island, as the registry keeps them (decision 090)
    std::string opened = "none";
    Ms now{ 0 };

    static WindowContent pages(const std::string& ids)
    {
        WindowContent content{ .title = "Window" };
        for (std::size_t from = 0; from <= ids.size();) {
            std::size_t comma = std::min(ids.find(',', from), ids.size());
            content.pages.push_back({ .id = ids.substr(from, comma - from) });
            from = comma + 1;
        }
        return content;
    }

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
                else if (w[i] == "awake") a.content.noIdleTimeout = true;
                else if (auto v = option(w[i], "by")) a.providerId = *v;
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
                else if (w[i] == "refresh") a.refreshOnly = true;
                else fail("unknown alert option '" + w[i] + "'");
            }
            apply(AlertPosted{ a });
        } else if (cmd == "click") apply(PillClicked{});
        else if (cmd == "outside") apply(ClickedOutside{});
        else if (cmd == "close-card" && w.size() > 1) apply(CardCloseRequested{ w[1] });
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
            p.content.keepOpen = arg(2) == "keep" || arg(2) == "awake";
            p.content.noIdleTimeout = arg(2) == "awake";
            if (arg(2) == "field" || arg(2).rfind("menu", 0) == 0) {
                p.content.expandedCenter = { row({ TextInput{ .actionId = "go" } }) };
                p.content.fieldFirst = "go";
            }
            if (arg(2).rfind("menu", 0) == 0) {
                std::vector<MenuItem> items{ { .label = "Open", .actionId = "open", .bold = true },
                                             { .label = "Run", .actionId = "run", .enabled = false },
                                             { .label = arg(2) == "menu=changed" ? "Copy" : "Reveal", .actionId = "reveal", .group = 1 } };
                for (const char* name : { "one", "two" }) p.content.expandedCenter.push_back(Row{ .items = { Text{ name } }, .menu = items });
            }
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
        else if (cmd == "toast") apply(ToastShown{ { .activityId = arg(1), .text = "Deleted", .actionLabel = "Undo", .actionId = "undo" } });
        else if (cmd == "toast-acted") apply(ToastClosed{});
        else if (cmd == "menu") {
            std::optional<Vec2> point;
            if (arg(3) == "pointer") point = Vec2{ 40, 60 };
            apply(MenuOpened{ arg(1), "expandedCenter/" + arg(2), point });
        }
        else if (cmd == "menu-step" || cmd == "menu-end" || cmd == "menu-letter") {
            if (!state.card.menu) fail(cmd + " with no menu open");
            else if (cmd == "menu-step") apply(MenuLit{ menuStep(*state.card.menu, std::stoi(arg(1))) });
            else if (cmd == "menu-end") apply(MenuLit{ menuEnd(*state.card.menu, arg(1) == "last") });
            else apply(MenuLit{ menuLetter(*state.card.menu, arg(1).empty() ? ' ' : arg(1)[0]) });
        }
        else if (cmd == "menu-light") apply(MenuLit{ arg(1) == "none" ? std::nullopt : std::optional<std::size_t>(std::stoul(arg(1))) });
        else if (cmd == "menu-close") apply(MenuClosed{});
        else if (cmd == "pin") apply(StayOpenToggled{});
        else if (cmd == "acted") userActed(windows, arg(1), now);
        else if (cmd == "open-window") {
            WindowSpec spec{ .id = arg(2) };
            if (auto v = option(arg(3), "page")) spec.page = *v;
            static const char* names[] = { "opened", "raised", "notnow", "toomany", "badid" };
            opened = names[static_cast<int>(openWindow(windows, arg(1), spec, pages("a,b,c"), now))];
        }
        else if (cmd == "update-window") updateWindow(windows, { arg(1), arg(2) }, pages(option(arg(3), "pages").value_or("a")));
        else if (cmd == "close-window") closeWindow(windows, { arg(1), arg(2) });
        else if (cmd == "pick-page") showPage(windows, { arg(1), arg(2) }, arg(3));
        else if (cmd == "stop-provider") closeProviderWindows(windows, arg(1));
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
            } else if (auto v = option(word, "toast")) {
                std::string got = state.card.toast ? state.card.toast->activityId : "none";
                if (got != *v) fail("toast=" + *v + ", got " + got);
            } else if (auto v = option(word, "menu")) {
                std::string open = state.card.menu ? state.card.menu->activityId + "/" + state.card.menu->row.substr(state.card.menu->row.find('/') + 1) : "none";
                if (open != *v) fail("menu=" + *v + ", got " + open);
            } else if (auto v = option(word, "lit")) {
                std::string lit = state.card.menu && state.card.menu->lit ? std::to_string(*state.card.menu->lit) : "none";
                if (lit != *v) fail("lit=" + *v + ", got " + lit);
            } else if (auto v = option(word, "windows")) {
                std::vector<std::string> open;
                for (const OpenWindow& o : windows.open) open.push_back(o.key.providerId + "/" + o.key.windowId);
                if (joined(open) != *v) fail("windows=" + *v + ", got " + joined(open));
            } else if (auto v = option(word, "front")) {
                const OpenWindow* front = nullptr;
                for (const OpenWindow& o : windows.open)
                    if (!front || o.raised > front->raised) front = &o;
                std::string got = front ? front->key.providerId + "/" + front->key.windowId : "none";
                if (got != *v) fail("front=" + *v + ", got " + got);
            } else if (auto v = option(word, "page")) {
                std::string key = v->substr(0, v->find(':'));
                std::string got = "none";
                for (const OpenWindow& o : windows.open)
                    if (o.key.providerId + "/" + o.key.windowId == key) got = o.page;
                if (key + ":" + got != *v) fail("page=" + *v + ", got " + got);
            } else if (auto v = option(word, "opened")) {
                if (opened != *v) fail("opened=" + *v + ", got " + opened);
            } else if (auto v = option(word, "squash")) {
                std::string got = pressSquashes(state) ? "yes" : "no";
                if (got != *v) fail("squash=" + *v + ", got " + got);
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
                std::string got = state.card.dialog ? state.card.dialog->activityId : "none";
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
