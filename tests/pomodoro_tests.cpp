// Pomodoro end to end (decision 009): registry + provider + reduce() +
// layout() + hitTest(), with wake-ups driven like the app's timer.

#include "core/dialog.h"
#include "core/hit_test.h"
#include "core/item_metrics.h"
#include "core/card_overlays.h"
#include "core/layout.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "core/window_fields.h"
#include "core/window_layout.h"
#include "expect.h"
#include "providers/media/now_playing.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"

#include <algorithm>
#include <cmath>

namespace cup::test {
namespace {

struct Island {
    IslandState state;
    Ms now{ 0 };
    ProviderRegistry registry;

    explicit Island(ProviderRegistry::Store store = {})
        : registry([this](const Event& e, const char*) { state = reduce(state, e, now); }, [this] { return now; }, std::move(store))
    {
    }

    Scene scene() const { return layout(state, now); }
    void send(const Event& e) { state = reduce(state, e, now); }

    // Click the button with this key, like the app does.
    bool press(const std::string& key)
    {
        Scene s = scene();
        for (const Placed& p : s.elements) {
            if (p.key != key) continue;
            auto hit = hitTest(s, { p.frame.x + p.frame.width / 2, p.frame.y + p.frame.height / 2 });
            if (!hit) return false;
            registry.route(hit->action);
            return true;
        }
        return false;
    }

    // The card's expand action (decision 106), as its button would send it: the
    // button is hidden while the window isn't ready (kPomodoroWindowReady).
    bool openWindow()
    {
        registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kOpenWindow, 0 });
        return true;
    }

    // The app's timer: advance to the provider's wake-up and deliver it.
    void wake()
    {
        if (auto at = registry.nextWake()) now = *at;
        registry.wakeDue(now);
        send(Tick{});
    }

    const Activity* focus() const { return findActivity(state, PomodoroProvider::kPanelId); }

    const Dialog* dialog() const { return dialogOver(state, PomodoroProvider::kPanelId); } // over its card

    // Its window (decision 106), and a click in it.
    const OpenWindow* window() const { return findWindow(registry.windows(), { "pomodoro", PomodoroProvider::kWindowId }); }
    void inWindow(const std::string& actionId) { registry.route({ "pomodoro", "", actionId, 0, "", PomodoroProvider::kWindowId }); }
    WindowScene windowScene() const { return windowLayout(*window(), { 1000, 2000 }, 0, now); }
};

const std::string kTile = std::string("tile/") + PomodoroProvider::kPanelId;
// The card (decision 107): a header, the time, the steps, the controls.
const std::string kStart = "expandedCenter/3/0"; // ▶, under the steps
const std::string kPause = "expandedCenter/3/1"; // the running card's controls: stop, ⏸, skip
const std::string kStop = "expandedCenter/3/0";
const std::string kTime = "expandedCenter/1/0";

const Placed* textIn(const WindowScene& scene, const std::string& value)
{
    for (const Placed& p : scene.page)
        if (auto* text = std::get_if<Text>(&p.item); text && text->value == value) return &p;
    return nullptr;
}

const Placed* buttonIn(const WindowScene& scene, const std::string& actionId)
{
    for (const Placed& p : scene.page)
        if (auto* button = std::get_if<Button>(&p.item); button && button->actionId == actionId) return &p;
    return nullptr;
}

const Placed* find(const Scene& scene, const std::string& key)
{
    for (const Placed& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

// The widget is information only (decision 019): nothing on it is a button.
bool noButtonsOn(const Scene& home)
{
    for (const Placed& p : home.elements)
        if (p.key.rfind(kTile, 0) == 0 && (std::holds_alternative<Button>(p.item) || std::holds_alternative<TextInput>(p.item))) return false;
    return true;
}

// The cards (decision 107): the phase and round over the time, big and
// heavy, centred; the phases as steps under it, then the controls; the
// window's buttons in the header's right end; nothing else.
void cardTests()
{
    const Tokens& t = tokens();
    Island island;
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    island.send(PillClicked{});
    island.send(TileClicked{ PomodoroProvider::kPanelId });
    Scene dormant = island.scene();
    const Placed* time = find(dormant, kTime);
    const Placed* start = find(dormant, kStart);
    expect(dormant.size.x == t.expandedSize.x, "as wide as every card: no wider timer card any more (decision 106 over 085)");
    expect(time && std::get<Text>(time->item).value == "25:00" && std::get<Text>(time->item).style == TextStyle::Hero, "the planned time, big and heavy");
    expect(time && std::abs(time->frame.x + time->frame.width / 2 - dormant.size.x / 2) < 1, "centred");
    expect(start && std::get<Button>(start->item).primary && std::get<Button>(start->item).actionId == pomodoro_action::kStart, "▶ under it");
    expect(std::none_of(dormant.elements.begin(), dormant.elements.end(), [](const Placed& p) {
               auto* b = std::get_if<Button>(&p.item);
               return b && b->actionId == pomodoro_action::kOpenWindow;
           }) == !kPomodoroWindowReady, "the window's button only once the window is ready");
    expect(std::none_of(dormant.elements.begin(), dormant.elements.end(), [](const Placed& p) {
               auto* b = std::get_if<Button>(&p.item);
               return p.key.rfind("expandedCenter/", 0) == 0 && b && (b->glyph == Glyph::Chart || b->glyph == Glyph::Settings);
           }), "no statistics or settings buttons on the island: the window has them");
    // The header's keys number its trailing (the window's button) before its leading (slot_layout).
    auto header = [](int i) { return "expandedCenter/0/" + std::to_string(i + (kPomodoroWindowReady ? 1 : 0)); };
    const Placed* name = find(dormant, header(1));
    const Placed* dot = find(dormant, header(0));
    expect(dot && name && std::get<Text>(name->item).value == "Focus" && dot->frame.x < name->frame.x && name->frame.y < time->frame.y,
           "the phase's dot and name over the time, at the left");
    for (const Placed& p : dormant.elements)
        expect(!std::holds_alternative<TextInput>(p.item) && !std::holds_alternative<Dial>(p.item),
               "no to-dos or dial on the card: the window has them: " + p.key);

    // Running: the steps say where the session is, a focus long, a break a dot.
    island.press(kStart);
    Scene running = island.scene();
    const Placed* steps = find(running, "expandedCenter/2/0");
    expect(steps && std::get<Steps>(steps->item) == phaseSteps(0) && std::get<Steps>(steps->item).count == phaseCount(), "a step per phase");
    const Steps& phases = std::get<Steps>(steps->item);
    expect(phases.lengths.front() == StepLength::Long && phases.lengths[1] == StepLength::Dot && phases.lengths.back() == StepLength::Short,
           "focus long, a break a dot, the long break short");
    expect(stepWidth(phases, 0) > stepWidth(phases, phaseCount() - 1) && stepWidth(phases, phaseCount() - 1) > stepWidth(phases, 1),
           "...and drawn so");
    const Placed* stop = find(running, kStop);
    const Placed* pause = find(running, kPause);
    const Placed* skip = find(running, "expandedCenter/3/2");
    expect(stop && pause && skip && std::get<Button>(stop->item).glyph == Glyph::Stop && std::get<Button>(pause->item).primary &&
           std::get<Button>(skip->item).glyph == Glyph::Next && stop->frame.x < pause->frame.x && pause->frame.x < skip->frame.x &&
           steps->frame.y < pause->frame.y, "stop, ⏸ (the filled circle) and skip, under the steps");
    const Placed* round = find(running, header(2));
    expect(round && std::get<Text>(round->item).value == "round 1 of " + std::to_string(t.pomodoroRounds), "the round beside the phase");
    for (const Placed& p : running.elements)
        expect(p.frame.y + p.frame.height <= running.size.y - t.expandedPadding + 0.01f, "fits the card: " + p.key);

    // One height with media's card and Home (decision 107): switching tabs doesn't resize the island.
    IslandState media;
    NowPlaying song{ .title = "Song", .artist = "Artist", .timeline = { .duration = Ms{ 200000 }, .playing = true } };
    media = reduce(media, ActivityStarted{ { .id = "media", .content = nowPlayingContent(song) } }, Ms{ 0 });
    media = reduce(media, PillClicked{}, Ms{ 0 });
    float mediaHeight = layout(media, Ms{ 0 }).size.y;
    auto cardHeight = [&](const Scene& s) { return s.size.y - (s.tabs ? tabStripSpace(island.state) : 0); };
    expect(cardHeight(running) == mediaHeight && cardHeight(dormant) == mediaHeight && mediaHeight == t.expandedSize.y,
           "dormant and running, the media card's height (" + std::to_string(int(mediaHeight)) + ")");
    island.send(HomeOpened{});
    expect(cardHeight(island.scene()) == mediaHeight, "and Home's");
    island.send(TileClicked{ PomodoroProvider::kPanelId });

    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });
}

// The window (decision 106): the expand button opens it, or brings it
// forward; its pages have what the card leaves out.
void windowTests()
{
    const Tokens& t = tokens();
    std::string saved, savedTasks;
    ProviderRegistry::Store store{
        .load = [&](const std::string&, const std::string& key) { return key == PomodoroProvider::kPlanKey ? saved : ""; },
        .save = [&](const std::string&, const std::string& key, const std::string& text) {
            if (key == PomodoroProvider::kPlanKey) saved = text;
            if (key == PomodoroProvider::kTodosKey) savedTasks = text;
        },
    };
    Island island(store);
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    island.send(PillClicked{});
    island.send(TileClicked{ PomodoroProvider::kPanelId });
    expect(!island.window(), "no window until asked");
    expect(island.openWindow() && island.window(), "the expand button opens the window");
    const OpenWindow& w = *island.window();
    expect(w.spec.size == WindowSize::Large && w.spec.resizable && w.spec.maximizable, "it resizes and maximizes (decision 117)");
    expect(w.content.rail && w.content.pages.size() == 3 && w.page == kPomodoroTimerPage && w.content.pages[1].id == kPomodoroStatsPage &&
           w.content.pages[2].id == kPomodoroSettingsPage && w.content.pages[2].atFoot, "three pages on a rail, the timer first, settings at its foot");
    std::uint64_t raised = w.raised;
    island.openWindow();
    expect(island.registry.windows().open.size() == 1 && island.window()->raised != raised, "again: the same window, brought forward");

    // The timer page: the time and ▶ across the top, then a card per group: "Tasks", its field, "New group".
    WindowScene timer = island.windowScene();
    expect(textIn(timer, "25:00") && buttonIn(timer, pomodoro_action::kStart) && textIn(timer, "Focus") &&
           !timer.headerRule, "the header: the planned focus, ▶, the phase and round");
    const Placed* bar = nullptr;
    for (const Placed& p : timer.page)
        if (p.key == "header/progress") bar = &p;
    expect(bar && std::holds_alternative<Progress>(bar->item) && bar->frame.x == 0 && bar->frame.width == timer.pane.width && bar->accent &&
               std::get<Progress>(bar->item).timeline.duration == Ms{ 25 * 60000 },
           "under it, the phase's progress, edge to edge, in the accent (decision 122)");
    expect(!std::get<Button>(buttonIn(timer, pomodoro_action::kStop)->item).enabled, "dormant: stop and skip dimmed, in their places");
    // One line (decision 120): the time and the controls on one middle; the phase and round a loose gap off the steps.
    const Placed* hero = textIn(timer, "25:00");
    const Placed* play = buttonIn(timer, pomodoro_action::kStart);
    auto middle = [](const Placed* p) { return p->frame.y + p->frame.height / 2; };
    expect(hero && play && std::abs(middle(hero) - middle(play)) < 0.5f, "the time and ▶ on one line");
    const Placed* where = textIn(timer, "Focus"); // only the phase (decision 122)
    const Placed* steps = nullptr;
    for (const Placed& p : timer.page)
        if (std::holds_alternative<Steps>(p.item)) steps = &p;
    expect(where && steps && std::get<Text>(where->item).alignEnd &&
               std::abs(steps->frame.x - (where->frame.x + where->frame.width) - t.looseItemSpacing) < 0.5f,
           "the round set against its end, the row's loose gap before the steps");
    expect(textIn(timer, "Tasks") && textIn(timer, "New group") && !textIn(timer, "Timer"), "the first group, the new group card; no page title");
    // "+ Add task" is a button that stays (decision 132): the whole row its click, no field until it's clicked.
    auto writingField = [](const WindowScene& scene) -> const Placed* {
        for (const Placed& p : scene.page)
            if (std::holds_alternative<TextInput>(p.item)) return &p;
        return nullptr;
    };
    const std::string writeTask = std::string(task_action::kWriteTask) + "1", taskWritten = std::string(task_action::kTaskWritten) + "1";
    const Placed* addTask = buttonIn(timer, writeTask);
    expect(addTask && textIn(timer, "Add task") && std::get<Button>(addTask->item).glyph == Glyph::Plus && addTask->litByRow && !writingField(timer),
           "Add task: '+ Add task', a button lit by its row, the whole row its click; no field yet");

    // ▶ in the window starts it; the card on the island follows.
    island.inWindow(pomodoro_action::kStart);
    expect(island.focus() && island.window(), "started from the window");
    timer = island.windowScene();
    expect(buttonIn(timer, pomodoro_action::kStop) && buttonIn(timer, pomodoro_action::kPause) && buttonIn(timer, pomodoro_action::kSkip) &&
           std::get<Button>(buttonIn(timer, pomodoro_action::kStop)->item).enabled, "running: its controls");

    // Stop asks with the host's question over the window (decision 126): Cancel keeps it, Stop ends it.
    island.inWindow(pomodoro_action::kStop);
    const OpenWindow* stopping = island.window();
    timer = island.windowScene();
    expect(island.focus() && !island.dialog() && stopping->dialog && stopping->dialog->title == "Stop the session?" &&
               stopping->dialog->confirmAction == pomodoro_action::kStopConfirmed && stopping->dialog->destructive && timer.dialog &&
               buttonIn(timer, pomodoro_action::kPause),
           "stop asks first, in a question over the window; the controls stay under it");
    island.inWindow(kDialogCancel);
    expect(island.focus() && !island.window()->dialog, "cancelled: still running");
    island.inWindow(pomodoro_action::kStop);
    island.inWindow(kDialogConfirm);
    expect(!island.focus() && !island.window()->dialog && buttonIn(island.windowScene(), pomodoro_action::kStart),
           "confirmed: stopped, the window dormant");
    // On the card, stop still asks with the island's dialog (decision 020).
    island.press(kStart);
    island.press(kStop);
    expect(island.dialog() && island.dialog()->title == "Stop the session?", "the card's stop: a dialog over the card");
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });
    island.send(DialogClosed{});

    // Tasks (decisions 105, 132): "Add task" opens a row under it, with the keys; Enter adds one and keeps them.
    WindowTyping typing;
    island.inWindow(writeTask);
    WindowScene page = island.windowScene();
    const Placed* row = writingField(page);
    addTask = buttonIn(page, writeTask);
    expect(row && addTask && row->frame.y > addTask->frame.y && std::get<TextInput>(row->item).actionId == inGroup(1, todo_action::kAdd),
           "clicked, Add task stays, a row being written under it");
    expect(focusArrived(typing, page) && typing.draft && typing.draft->actionId == inGroup(1, todo_action::kAdd), "...with the keys at once");
    typeText(typing, page, "Write the report");
    FieldOutcome enter = cup::fieldKey(typing, page, FieldKey::Enter, false, false);
    expect(enter.send && enter.send->windowId == PomodoroProvider::kWindowId && !enter.letGo, "Enter in the row sends the task, keeping the keys");
    if (enter.send) island.registry.route(*enter.send);
    page = island.windowScene();
    const Placed* task = textIn(page, "Write the report");
    expect(task && std::get<Text>(task->item).editAction == inGroup(1, std::string(todo_action::kEdit) + "1"), "added: in its group, edited in place");
    expect(writingField(page) && !focusArrived(typing, page) && typing.draft, "...and the row stays, for the next one");
    // Written, it's drawn as the task it adds (decision 125): a task row's height, a box, no field box.
    WindowScene written = windowLayout(*island.window(), { 1000, 2000 }, 0, island.now, {}, &typing);
    const Placed *addField = writingField(written), *box = buttonIn(written, inGroup(1, std::string(todo_action::kToggle) + "1"));
    const TextInput* shown = addField ? std::get_if<TextInput>(&addField->item) : nullptr;
    expect(shown && shown->focused && shown->focusedIcon == Glyph::Unchecked && addField->litByRow && box &&
               addField->frame.height == box->frame.height && addField->frame.x == box->frame.x && addField->listRow &&
               std::abs(addField->listRow->height - box->listRow->height) < 0.01f,
           "the row being written: a task row's height, its box where a task's is");
    // Left without Enter (a click elsewhere), what's written is added and the row goes (decisions 127, 132).
    typeText(typing, written, "Left without Enter");
    std::optional<Action> left = blurField(typing, written);
    expect(left && left->actionId == taskWritten && left->text == "Left without Enter" && !typing.draft, "leaving the row sends what's in it");
    if (left) island.registry.route(*left);
    page = island.windowScene();
    expect(textIn(page, "Left without Enter") && !writingField(page), "...added, and the row gone");
    // Left blank, nothing is added and the row goes; Esc drops what's in it.
    island.inWindow(writeTask);
    page = island.windowScene();
    focusArrived(typing, page);
    typeText(typing, page, "   ");
    left = blurField(typing, page);
    if (left) island.registry.route(*left);
    page = island.windowScene();
    expect(left && !writingField(page) && typing.kept.empty() && island.window() && savedTasks.find("[ ]    ") == std::string::npos,
           "left blank: nothing added, nothing kept, the row gone");
    island.inWindow(writeTask);
    page = island.windowScene();
    focusArrived(typing, page);
    typeText(typing, page, "Dropped");
    FieldOutcome esc = cup::fieldKey(typing, page, FieldKey::Escape, false, false);
    if (esc.send) island.registry.route(*esc.send);
    page = island.windowScene();
    expect(esc.letGo && esc.send && esc.send->text.empty() && !writingField(page) && !textIn(page, "Dropped"), "Esc: dropped, the row gone");
    island.inWindow(inGroup(1, std::string(todo_action::kDelete) + "2")); // back to the one task the rest of the test expects
    expect(textIn(page, "1") == nullptr, "no count beside the group's name (decision 121)");
    const Placed* remove = buttonIn(page, inGroup(1, std::string(todo_action::kDelete) + "1"));
    expect(remove && std::get<Button>(remove->item).revealOnHover, "its delete shows on hover");
    island.inWindow(inGroup(1, std::string(todo_action::kDelete) + "1"));
    page = island.windowScene();
    // Its Undo is the host's toast over the window (decision 121), not a row of the page.
    auto toastSays = [&](const std::string& text) {
        const OpenWindow* w = island.window();
        if (!w || !w->toast || w->toast->text != text || w->toast->actionId != kPomodoroUndo) return false;
        WindowScene shown = island.windowScene();
        return shown.toast.has_value() && !textIn(shown, text);
    };
    expect(!textIn(page, "Write the report") && toastSays("Task deleted"), "deleted, with a toast over the window: Undo");
    island.inWindow(kPomodoroUndo);
    page = island.windowScene();
    expect(textIn(page, "Write the report") && !island.window()->toast, "undone: back, the toast gone");
    expect(savedTasks == "# Tasks\n[ ] Write the report\n", "kept: the group's name over its tasks");

    // Groups: a new one, renamed in place; deleted with an Undo; the last one deleted leaves a fresh "Tasks".
    island.inWindow(task_action::kNewGroup);
    page = island.windowScene();
    expect(textIn(page, "Group 2") != nullptr, "a new group, named after the others");
    const Placed* name = textIn(page, "Group 2");
    expect(name && std::get<Text>(name->item).editAction == std::string(task_action::kRenameGroup) + "2", "its name edits in place");
    // Made, its name gets the keys at once, all of it selected: typed over (decision 134).
    const WindowContent& made = island.window()->content;
    WindowTyping naming;
    focusByAction(naming, page, made.focus);
    expect(made.focus == std::string(task_action::kRenameGroup) + "2" && made.focusRequest > 0 && naming.draft &&
               naming.draft->actionId == made.focus && naming.draft->text == "Group 2" && naming.draft->anchor == 0 && naming.draft->caret == 7,
           "a new group asks for its name to be focused, selected");
    island.registry.route({ "pomodoro", "", std::string(task_action::kRenameGroup) + "2", 0, "Work", PomodoroProvider::kWindowId });
    page = island.windowScene();
    expect(textIn(page, "Work") && !textIn(page, "Group 2"), "renamed");
    // A group with tasks asks first, in a question over the window (decision 122): Cancel keeps it.
    island.inWindow(std::string(task_action::kDeleteGroup) + "1");
    const OpenWindow* asking = island.window();
    expect(asking->dialog && asking->dialog->title == "Delete \u201CTasks\u201D?" && asking->dialog->message == "Its 1 task goes with it." &&
               asking->dialog->destructive && textIn(island.windowScene(), "Write the report"),
           "deleting a group with tasks asks first, its tasks still there");
    WindowScene asked = island.windowScene();
    expect(asked.dialog && windowRowAt(asked, { asked.pane.x + 100, asked.pane.y + 200 }).empty(), "a question over the window: nothing under it lit");
    island.inWindow(kDialogCancel);
    expect(!island.window()->dialog && textIn(island.windowScene(), "Write the report"), "Cancel: kept");
    island.inWindow(std::string(task_action::kDeleteGroup) + "1");
    island.inWindow(kDialogConfirm);
    page = island.windowScene();
    expect(!island.window()->dialog && !textIn(page, "Write the report") && textIn(page, "Work") && !island.window()->toast,
           "Delete: the group and its tasks gone; confirmed, no Undo toast");
    // An empty group goes at once, with Undo; the last one leaves a fresh "Tasks".
    island.inWindow(std::string(task_action::kDeleteGroup) + "2");
    page = island.windowScene();
    expect(!island.window()->dialog && textIn(page, "Tasks") && !textIn(page, "Work") && toastSays("\u201CWork\u201D deleted"),
           "an empty group: no question, deleted at once with a toast; the last one leaves a fresh Tasks");
    island.inWindow(kPomodoroUndo);
    page = island.windowScene();
    expect(textIn(page, "Work") && !textIn(page, "Tasks"), "undone: Work back, the stand-in gone");

    // The settings page: the plan (Focus  − 25 min +), then the days off.
    island.registry.windowPageShown({ "pomodoro", PomodoroProvider::kWindowId }, kPomodoroSettingsPage);
    WindowScene settings = island.windowScene();
    expect(textIn(settings, "Focus") && textIn(settings, "25 min") && textIn(settings, "5 min") && textIn(settings, "Days off"), "the plan and the days off");
    const Placed* minus = buttonIn(settings, pomodoro_action::kFocusLess);
    const Placed* plus = buttonIn(settings, pomodoro_action::kFocusMore);
    expect(minus && plus && std::get<Button>(minus->item).glyph == Glyph::Minus && std::get<Button>(minus->item).secondary &&
           minus->frame.x < plus->frame.x && minus->frame.y == plus->frame.y, "− and + around the length, grey");
    const Placed* mon = buttonIn(settings, std::string(pomodoro_action::kToggleDayOff) + "0");
    const Placed* sun = buttonIn(settings, std::string(pomodoro_action::kToggleDayOff) + "6");
    expect(mon && sun && mon->frame.y == sun->frame.y && std::get<Button>(mon->item).round, "the days: seven circles in one row");
    island.inWindow(pomodoro_action::kFocusMore);
    island.inWindow(pomodoro_action::kFocusMore);
    island.inWindow(pomodoro_action::kBreakLess);
    settings = island.windowScene();
    expect(textIn(settings, "35 min") && textIn(settings, "4 min"), "each click a step");
    Scene onIsland = island.scene();
    const Placed* card = find(onIsland, kTime);
    expect(card && std::get<Text>(card->item).value == "35:00", "the card's time follows");
    expect(loadPlan(saved).focus == Ms{ 35 * 60000 } && loadPlan(saved).rest == Ms{ 4 * 60000 }, "kept in the store");
    island.inWindow(std::string(pomodoro_action::kToggleDayOff) + "6");
    expect(std::get<Button>(buttonIn(island.windowScene(), std::string(pomodoro_action::kToggleDayOff) + "6")->item).on == true, "Sunday off");
    expect(island.press(kStart) && island.registry.nextWake() == Ms{ 35 * 60000 }, "started: focus lasts the planned time");
    expect(!std::get<Button>(buttonIn(island.windowScene(), pomodoro_action::kFocusMore)->item).enabled, "while it runs, the plan is dimmed");
    island.wake();
    expect(island.focus() && island.focus()->content.label == "Break" && island.registry.nextWake() == island.now + Ms{ 4 * 60000 },
           "and the break too");
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });

    // The user closes it: gone; the expand button opens it again on its timer page.
    island.registry.windowClosed({ "pomodoro", PomodoroProvider::kWindowId });
    expect(!island.window(), "closed");
    island.send(HomeOpened{});
    island.send(TileClicked{ PomodoroProvider::kPanelId });
    expect(island.openWindow() && island.window(), "opened again");

    // Stopping the provider closes it.
    island.registry.stop("pomodoro");
    expect(!island.window(), "stopping the provider closes its window");

    // Restarted: the plan is back.
    Island again(store);
    again.registry.add(std::make_unique<PomodoroProvider>());
    again.registry.start("pomodoro");
    const Panel* panel = findPanel(again.state, PomodoroProvider::kPanelId);
    expect(panel && std::get<Text>(panel->content.tile[0].items[0]).value == "35:00", "after a restart: the widget shows the planned focus");
}

} // namespace

void runPomodoroTests()
{
    cardTests();
    windowTests();
    const Tokens& t = tokens();
    Island island;
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    expect(island.state.panels.size() == 1 && island.state.panels[0].providerId == "pomodoro", "starting publishes the panel");
    expect(mode(island.state) == Mode::Idle && !island.registry.nextWake(), "dormant: idle, nothing scheduled");

    // The dormant widget: the timer icon, the focus time, the plan; a click opens the card.
    island.send(PillClicked{});
    expect(island.state.homeOpen, "idle click opens Home");
    Scene dormantHome = island.scene();
    const Placed* icon = find(dormantHome, kTile + "/lead/0/0");
    const Placed* plannedTime = find(dormantHome, kTile + "/0/0");
    expect(icon && std::get<Icon>(icon->item).glyph == Glyph::Timer, "dormant widget: the timer icon on the left");
    expect(plannedTime && std::get<Text>(plannedTime->item).value == "25:00", "and the focus time");
    expect(noButtonsOn(dormantHome) && dormantHome.tiles.size() == 1 && dormantHome.tiles[0].opens, "no buttons: it opens the card");

    // Start from the card. A click on its empty space doesn't close it (Content::keepOpen).
    island.send(TileClicked{ PomodoroProvider::kPanelId });
    island.send(PillClicked{});
    expect(island.state.expandedId == PomodoroProvider::kPanelId, "dormant card: a click between its controls keeps it open");
    expect(reduce(island.state, Tick{}, island.now + t.expandedIdleTimeout * 10).expandedId == PomodoroProvider::kPanelId,
           "and left alone, it stays (decision 062)");
    expect(island.press(kStart), "the dormant card's start button");
    expect(island.focus() && island.focus()->content.label == "Focus", "start: a focus phase is live");
    expect(island.registry.nextWake() == Ms{ t.pomodoroFocus }, "wakes when focus ends");
    expect(island.state.expandedId == PomodoroProvider::kPanelId, "the live card shows");
    expect(island.openWindow() && island.window(), "its window, for the rest");
    island.send(PillClicked{});
    expect(island.state.expandedId == PomodoroProvider::kPanelId, "running card: a click between its controls keeps it open");
    expect(reduce(island.state, Tick{}, island.now + t.expandedIdleTimeout * 2).expandedId == PomodoroProvider::kPanelId,
           "the running card stays too when left alone");

    // The running widget: the ring filling, the time left, the phase and round.
    island.send(HomeOpened{});
    Scene home = island.scene();
    const Placed* left = find(home, kTile + "/0/0");
    const Placed* tileRing = find(home, kTile + "/lead/0/0");
    const Placed* glance = find(home, kTile + "/1/0");
    expect(home.tiles.size() == 1 && home.tiles[0].frame == dormantHome.tiles[0].frame, "the widget stays in place");
    expect(left && std::get<Text>(left->item).value == "25:00", "the widget counts down");
    expect(tileRing && std::get<Progress>(tileRing->item).shape == ProgressShape::Ring &&
           tileRing->frame.height == home.tiles[0].frame.height - 2 * t.tilePadding, "a ring as tall as the widget");
    expect(glance && std::get<Text>(glance->item).value == "Focus · 1/" + std::to_string(t.pomodoroRounds), "the phase and round");
    expect(noButtonsOn(home), "still no buttons");

    // Compact: the countdown in the middle, the ring on the right, no phase name; red for focus.
    island.send(EscapePressed{});
    Scene compact = island.scene();
    const Placed* centre = nullptr;
    const Placed* ring = nullptr;
    for (const Placed& p : compact.elements) {
        if (p.key == "compactCenter/0/0") centre = &p;
        if (p.key == "compactTrailing/0/0") ring = &p;
        if (auto* text = std::get_if<Text>(&p.item)) expect(text->value != "Focus", "compact shows no phase name");
    }
    expect(centre && std::get<Text>(centre->item).value == "25:00", "compact: the countdown in the centre");
    expect(ring && std::get<Progress>(ring->item).shape == ProgressShape::Ring, "compact: the ring on the right");
    Color focusRed = *island.focus()->content.accent;
    expect(ring && ring->accent == focusRed, "the ring takes the phase's accent");

    // How many phases have passed: in the window (decision 106).
    auto stepsShown = [&] {
        if (const OpenWindow* w = island.window())
            for (const Row& r : w->content.pages[0].header)
                for (const Item& item : r.trailing)
                    if (auto* steps = std::get_if<Steps>(&item)) return *steps;
        return Steps{ -1, -1 };
    };
    expect(stepsShown() == phaseSteps(0) && stepsShown().count == static_cast<int>(2 * t.pomodoroRounds), "the window: a step per phase, none done yet");

    // The card: the time left, ticking, beside the ring filling with the phase.
    island.send(PillClicked{});
    Scene card = island.scene();
    island.send(EscapePressed{});
    const Placed* bigTimer = find(card, kTime);
    const Placed* cardSteps = find(card, "expandedCenter/2/0");
    expect(bigTimer && std::get<Text>(bigTimer->item).value == "25:00" && bigTimer->ticking, "card: the time left, ticking");
    expect(cardSteps && cardSteps->accent == focusRed && cardSteps->frame.y > bigTimer->frame.y, "the steps under it, in the phase's colour");

    // A phase ends: alert, next phase.
    island.send(EscapePressed{});
    island.wake();
    expect(island.focus() && island.focus()->content.label == "Break", "focus over: a break");
    expect(mode(island.state) == Mode::Alert && island.state.alert->content.label == "Break time" &&
           island.state.alert->level == Level::TimeSensitive, "with a time-sensitive alert");
    expect(island.registry.nextWake() == island.now + t.pomodoroBreak, "wakes when the break ends");
    expect(island.focus()->content.accent && *island.focus()->content.accent != focusRed, "a break has its own colour");
    island.now += t.alertDuration;
    island.send(Tick{});
    expect(stepsShown().done == 1, "one phase done");

    // Skip: next phase now, no alert.
    island.now += Ms{ 10000 };
    island.send(Tick{});
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kSkip, 0 });
    expect(island.focus()->content.label == "Focus" && island.state.alertQueue.empty() && !island.state.alert, "skip: back to focus, quietly");

    // Pause holds the clock; resume picks up where it was.
    island.now += Ms{ 60000 };
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kPause, 0 });
    expect(!island.registry.nextWake(), "paused: no wake-up");
    const Slot& pausedTile = island.focus()->content.tile;
    expect(pausedTile.size() == 2 && pausedTile[1].items[0] == Item{ Text{ "Paused · 2/" + std::to_string(t.pomodoroRounds), TextStyle::Body } },
           "the widget says it's paused");
    island.now += Ms{ 600000 };
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kPause, 0 });
    expect(island.registry.nextWake() == island.now + t.pomodoroFocus - Ms{ 60000 }, "resumed: the rest of the phase");

    // Natural exit: the rounds run out after the long break.
    std::string lastAlert;
    for (int guard = 0; guard < 20 && island.focus(); ++guard) {
        island.wake();
        if (island.state.alert) lastAlert = island.state.alert->content.label;
        island.now += t.alertDuration;
        island.send(Tick{});
    }
    expect(!island.focus() && lastAlert == "Session done", "the session ends by itself, and says so");
    expect(island.state.panels.size() == 1 && !island.registry.nextWake(), "the panel is dormant again");

    // Manual exit: the stop button on the card, once confirmed (decision 020).
    island.send(PillClicked{});
    island.send(TileClicked{ PomodoroProvider::kPanelId });
    island.press(kStart);
    expect(island.focus() != nullptr, "started again");
    expect(island.press(kStop), "the running card's stop button");
    expect(island.focus() && island.dialog() && island.dialog()->title == "Stop the session?" &&
           island.dialog()->message == "You're in round 1 of " + std::to_string(t.pomodoroRounds) + ". The timer starts over next time." &&
           island.dialog()->destructive, "stop asks first; still running");
    island.send(DialogClosed{}); // Cancel
    expect(island.focus() && !island.dialog(), "cancelled: still running");
    island.press(kStop);
    Dialog asked = *island.dialog();
    island.registry.route({ asked.providerId, asked.activityId, asked.confirmAction, 0 }); // the app: Stop clicked
    island.send(DialogClosed{});
    expect(!island.focus() && !island.registry.nextWake() && cardOnScreen(island.state, PomodoroProvider::kPanelId),
           "confirmed: dormant, its card still open");
    PomodoroProvider::Session longBreak{ .phase = PomodoroProvider::Phase::LongBreak, .round = t.pomodoroRounds };
    expect(stopQuestion(longBreak).message == "You're on the long break. The timer starts over next time.", "on the long break, says so");

    // Ctrl+W (decision 034): only a live activity's card has something to
    // close; on Pomodoro's it asks like the stop button, then stops.
    expect(!shownActivity(island.state), "ctrl+w: the dormant card has no activity to close");
    island.press(kStart);
    auto shown = shownActivity(island.state);
    expect(shown && shown->activityId == PomodoroProvider::kPanelId, "ctrl+w: the running card's activity");
    island.registry.closeRequested(*shown);
    expect(island.focus() && island.dialog() && island.dialog()->title == "Stop the session?", "ctrl+w: asks first; still running");
    asked = *island.dialog();
    island.registry.route({ asked.providerId, asked.activityId, asked.confirmAction, 0 });
    island.send(DialogClosed{});
    expect(!island.focus() && cardOnScreen(island.state, PomodoroProvider::kPanelId), "ctrl+w: confirmed, stopped; its card still open");
    island.send(HomeOpened{});
    expect(!shownActivity(island.state), "ctrl+w: Home has no activity to close");
    island.send(TileClicked{ PomodoroProvider::kPanelId });

    island.press(kStart);
    island.registry.stop("pomodoro");
    expect(island.state.panels.empty() && !island.focus(), "stopping the provider removes the panel and the session");
    expect(mode(island.state) == Mode::Idle, "and closes Home");

    // A restart (decisions 027, 061): the session is saved, and a new run
    // has it back paused, where it was when the app closed.
    std::string saved;
    auto storage = [&] {
        return ProviderRegistry::Store{
            .load = [&](const std::string&, const std::string& key) { return key == PomodoroProvider::kSessionKey ? saved : ""; },
            .save = [&](const std::string&, const std::string& key, const std::string& s) { if (key == PomodoroProvider::kSessionKey) saved = s; } };
    };
    auto run = std::make_unique<Island>(storage());
    run->registry.add(std::make_unique<PomodoroProvider>());
    run->registry.start("pomodoro");
    run->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStart, 0 });
    run->now += Ms{ 60000 };
    run->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kPause, 0 }); // 1 min in, paused
    run.reset(); // the app quits
    auto next = std::make_unique<Island>(storage());
    next->now = Ms{ 500 }; // a new run: the app's own clock started over
    next->registry.add(std::make_unique<PomodoroProvider>());
    next->registry.start("pomodoro");
    const Activity* resumed = next->focus();
    expect(resumed && resumed->content.label == "Focus" && !next->registry.nextWake(), "restarted: the paused session is back, still paused");
    run = std::move(next);
    run->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kPause, 0 }); // resume: running again
    expect(run->registry.nextWake() == run->now + t.pomodoroFocus - Ms{ 60000 }, "resumed: the same minute left to go");

    // Running when the app quits: back paused at that moment, however long it was closed.
    run->now += Ms{ 120000 };
    run.reset();
    next = std::make_unique<Island>(storage());
    next->registry.add(std::make_unique<PomodoroProvider>());
    next->registry.start("pomodoro");
    expect(next->focus() && next->focus()->content.label == "Focus" && !next->registry.nextWake() && !next->state.alert,
           "quit while running: paused on the same phase, no alert");
    next->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kPause, 0 });
    expect(next->registry.nextWake() == next->now + t.pomodoroFocus - Ms{ 180000 }, "three minutes in, as when it quit");

    // Stopping forgets it.
    next->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });
    expect(saved.empty() && !resumeSession(saved, Ms{ 0 }), "stopped: nothing to resume");
    expect(!resumeSession("garbage", Ms{ 0 }) && !resumeSession("pomodoro2 7 1 0 1000 0", Ms{ 0 }), "unreadable: nothing");
    std::optional<PomodoroSession> old = resumeSession("pomodoro1 1 2 1 1000 60000 5000", Ms{ 7 });
    expect(old && old->phase == PomodoroPhase::Break && old->round == 2 && old->timeline.position == Ms{ 1000 } && !old->timeline.playing &&
           old->timeline.anchoredAt == Ms{ 7 }, "decision 027's line: back paused where it was saved");
    PomodoroSession skips{ .phase = PomodoroPhase::Focus, .round = 3, .timeline = { .position = Ms{ 5 }, .duration = Ms{ 60000 } }, .focusSkipped = 1 };
    std::optional<PomodoroSession> again = resumeSession(saveSession(skips, Ms{ 0 }), Ms{ 0 });
    expect(again && again->focusSkipped == 1 && again->round == 3 && again->timeline.position == Ms{ 5 }, "the skips are kept");
}

} // namespace cup::test
