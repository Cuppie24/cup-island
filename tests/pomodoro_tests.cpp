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
const std::string kStart = "expandedBottom/1/0"; // ▶, under the bar (decision 106)
const std::string kPause = "expandedBottom/1/1"; // the running card's controls: stop, ⏸, skip
const std::string kStop = "expandedBottom/1/0";
const std::string kExpand = "expandedTrailing/0/0"; // the window, from the top-right corner

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

// The cards (decision 106): laid out like media's and as big, the ring where
// the cover goes, the time beside it, a bar and the controls under them,
// the expand button in the corner; nothing else.
void cardTests()
{
    const Tokens& t = tokens();
    Island island;
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    island.send(PillClicked{});
    island.send(TileClicked{ PomodoroProvider::kPanelId });
    Scene dormant = island.scene();
    const Placed* ring = find(dormant, "expandedLeading/0/0");
    const Placed* time = find(dormant, "expandedCenter/0/0");
    const Placed* start = find(dormant, kStart);
    const Placed* expand = find(dormant, kExpand);
    expect(dormant.size.x == t.expandedSize.x, "as wide as every card: no wider timer card any more (decision 106 over 085)");
    expect(ring && std::get<Progress>(ring->item).shape == ProgressShape::Ring && ring->frame.width == t.expandedArtSize,
           "the ring where media's cover goes");
    expect(time && std::get<Text>(time->item).value == "25:00" && std::get<Text>(time->item).style == TextStyle::Display, "the planned time, big");
    expect(start && std::get<Button>(start->item).primary && std::get<Button>(start->item).actionId == pomodoro_action::kStart, "▶ under them");
    expect(expand && std::get<Button>(expand->item).glyph == Glyph::Expand && std::get<Button>(expand->item).actionId == pomodoro_action::kOpenWindow,
           "the expand button in the top-right corner");
    for (const Placed& p : dormant.elements)
        expect(!std::holds_alternative<TextInput>(p.item) && !std::holds_alternative<Steps>(p.item) && !std::holds_alternative<Dial>(p.item),
               "no to-dos, dots or dial on the card: the window has them: " + p.key);

    // Running, it's media's size exactly, with or without a tab strip.
    island.press(kStart);
    Scene running = island.scene();
    IslandState media;
    NowPlaying song{ .title = "Song", .artist = "Artist", .timeline = { .duration = Ms{ 200000 }, .playing = true } };
    media = reduce(media, ActivityStarted{ { .id = "media", .content = nowPlayingContent(song) } }, Ms{ 0 });
    media = reduce(media, PillClicked{}, Ms{ 0 });
    Scene mediaCard = layout(media, Ms{ 0 });
    float strip = running.tabs ? tabStripSpace(island.state) : 0; // Home's tab row over it, while the panel is there
    expect(running.size.x == mediaCard.size.x && running.size.y - strip == mediaCard.size.y && !mediaCard.tabs,
           "the running card is the media card's size (" + std::to_string(int(mediaCard.size.x)) + " x " + std::to_string(int(mediaCard.size.y)) + ")");
    expect(dormant.size.y - (dormant.tabs ? tabStripSpace(island.state) : 0) == mediaCard.size.y, "the dormant one too");
    const Placed* stop = find(running, kStop);
    const Placed* pause = find(running, kPause);
    const Placed* skip = find(running, "expandedBottom/1/2");
    expect(stop && pause && skip && std::get<Button>(stop->item).glyph == Glyph::Stop && std::get<Button>(pause->item).primary &&
           std::get<Button>(skip->item).glyph == Glyph::Next && stop->frame.x < pause->frame.x && pause->frame.x < skip->frame.x,
           "stop, ⏸ (the filled circle) and skip, like media's previous, play and next");
    const Placed* what = find(running, "expandedCenter/1/0");
    expect(what && std::get<Text>(what->item).value == "Focus · round 1 of " + std::to_string(t.pomodoroRounds), "the phase and round under the time");
    for (const Placed& p : running.elements)
        expect(p.frame.y + p.frame.height <= running.size.y - t.expandedPadding + 0.01f, "fits the card: " + p.key);
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });
}

// The window (decision 106): the expand button opens it, or brings it
// forward; its pages have what the card leaves out.
void windowTests()
{
    const Tokens& t = tokens();
    std::string saved;
    ProviderRegistry::Store store{
        .load = [&](const std::string&, const std::string& key) { return key == PomodoroProvider::kPlanKey ? saved : ""; },
        .save = [&](const std::string&, const std::string& key, const std::string& text) { if (key == PomodoroProvider::kPlanKey) saved = text; },
    };
    Island island(store);
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    island.send(PillClicked{});
    island.send(TileClicked{ PomodoroProvider::kPanelId });
    expect(!island.window(), "no window until asked");
    expect(island.press(kExpand) && island.window(), "the expand button opens the window");
    const OpenWindow& w = *island.window();
    expect(w.spec.size == WindowSize::Large && !w.spec.resizable && !w.spec.maximizable, "one size, like Settings' window");
    expect(w.content.pages.size() == 3 && w.page == kPomodoroTimerPage && w.content.pages[1].id == kPomodoroStatsPage &&
           w.content.pages[2].id == kPomodoroSettingsPage, "three pages, the timer first");
    std::uint64_t raised = w.raised;
    island.press(kExpand);
    expect(island.registry.windows().open.size() == 1 && island.window()->raised != raised, "again: the same window, brought forward");

    // The timer page: the dial, the plan in words, ▶, today, the to-dos with their field.
    WindowScene timer = island.windowScene();
    const Placed* dial = nullptr;
    for (const Placed& p : timer.page)
        if (std::holds_alternative<Dial>(p.item)) dial = &p;
    expect(dial && std::get<Dial>(dial->item).shown == "25:00" && std::get<Dial>(dial->item).label == "Focus", "the dial, the planned focus in it");
    expect(textIn(timer, "Focus · " + std::to_string(t.pomodoroRounds) + " rounds of 25 min") && textIn(timer, "0 cycles") &&
           textIn(timer, "Today") && textIn(timer, "To-dos"), "the plan in words, today, the to-dos' label");
    const Placed* field = nullptr;
    for (const Placed& p : timer.page)
        if (auto* input = std::get_if<TextInput>(&p.item); input && input->actionId == todo_action::kAdd) field = &p;
    expect(field != nullptr, "the to-do field, in the window");

    // ▶ in the window starts it; the card on the island follows.
    island.inWindow(pomodoro_action::kStart);
    expect(island.focus() && island.window(), "started from the window");
    timer = island.windowScene();
    const Steps* steps = nullptr;
    for (const Placed& p : timer.page)
        if (auto* s = std::get_if<Steps>(&p.item)) steps = s;
    expect(steps && *steps == Steps{ phaseCount(), 0 } && textIn(timer, "0 of " + std::to_string(phaseCount()) + " done"),
           "running: a dot per phase, and how many are done");
    expect(buttonIn(timer, pomodoro_action::kStop) && buttonIn(timer, pomodoro_action::kPause) && buttonIn(timer, pomodoro_action::kSkip),
           "its controls");

    // Stop asks in place (a window has no dialogs over it): Cancel keeps it, Stop ends it.
    island.inWindow(pomodoro_action::kStop);
    timer = island.windowScene();
    expect(island.focus() && !island.dialog() && textIn(timer, "Stop the session?") && buttonIn(timer, pomodoro_action::kStopConfirmed) &&
           buttonIn(timer, pomodoro_action::kCancel) && !buttonIn(timer, pomodoro_action::kPause), "stop asks first, in the window, in the controls' place");
    island.inWindow(pomodoro_action::kCancel);
    expect(island.focus() && buttonIn(island.windowScene(), pomodoro_action::kPause), "cancelled: still running, the controls back");
    island.inWindow(pomodoro_action::kStop);
    island.registry.windowPageShown({ "pomodoro", PomodoroProvider::kWindowId }, kPomodoroStatsPage);
    island.registry.windowPageShown({ "pomodoro", PomodoroProvider::kWindowId }, kPomodoroTimerPage);
    expect(!textIn(island.windowScene(), "Stop the session?"), "another page: the question goes unanswered");
    island.inWindow(pomodoro_action::kStop);
    island.inWindow(pomodoro_action::kStopConfirmed);
    expect(!island.focus() && buttonIn(island.windowScene(), pomodoro_action::kStart), "confirmed: stopped, the window dormant");
    // On the card, stop still asks with the island's dialog (decision 020).
    island.press(kStart);
    island.press(kStop);
    expect(island.dialog() && island.dialog()->title == "Stop the session?", "the card's stop: a dialog over the card");
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });
    island.send(DialogClosed{});

    // The to-dos, typed into the window's field (decision 105): Enter adds one; a delete has its Undo in place; clearing asks in place.
    WindowTyping typing;
    WindowScene page = island.windowScene();
    std::string fieldKey;
    for (const Placed& p : page.page)
        if (auto* input = std::get_if<TextInput>(&p.item); input && input->actionId == todo_action::kAdd) fieldKey = p.key;
    focusField(typing, page, fieldKey, std::nullopt);
    typeText(typing, page, "Write the report");
    FieldOutcome enter = fieldKey.empty() ? FieldOutcome{} : cup::fieldKey(typing, page, FieldKey::Enter, false, false);
    expect(enter.send && enter.send->windowId == PomodoroProvider::kWindowId, "Enter in the window's field sends the to-do");
    if (enter.send) island.registry.route(*enter.send);
    page = island.windowScene();
    expect(textIn(page, "Write the report") != nullptr, "added: it's in the window's list");
    std::string id = std::to_string(1);
    island.inWindow(std::string(todo_action::kDelete) + id);
    page = island.windowScene();
    expect(!textIn(page, "Write the report") && textIn(page, "To-do deleted") && buttonIn(page, todo_action::kUndoDelete), "deleted, with an Undo row");
    island.inWindow(todo_action::kUndoDelete);
    page = island.windowScene();
    expect(textIn(page, "Write the report") && !textIn(page, "To-do deleted"), "undone: back, the row gone");
    island.inWindow(todo_action::kClearAll);
    page = island.windowScene();
    expect(!island.dialog() && textIn(page, "Clear all tasks?") && buttonIn(page, todo_action::kClearAllConfirmed), "clear all asks in place");
    island.inWindow(todo_action::kClearAllConfirmed);
    page = island.windowScene();
    expect(!textIn(page, "Write the report") && !textIn(page, "Clear all tasks?"), "confirmed: cleared, the question gone");

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
    const Placed* card = find(onIsland, "expandedCenter/0/0");
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
    expect(island.press(kExpand) && island.window(), "opened again");

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
    expect(island.press(kExpand) && island.window(), "its window, for the rest");
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
            for (const WindowSection& section : w->content.pages[0].sections)
                for (const Row& r : section.rows)
                    for (const Item& item : r.items)
                        if (auto* steps = std::get_if<Steps>(&item)) return *steps;
        return Steps{ -1, -1 };
    };
    expect(stepsShown() == Steps{ static_cast<int>(2 * t.pomodoroRounds), 0 }, "the window: a dot per phase, none done yet");

    // The card: the time left, ticking, beside the ring filling with the phase.
    island.send(PillClicked{});
    Scene card = island.scene();
    island.send(EscapePressed{});
    const Placed* bigTimer = find(card, "expandedCenter/0/0");
    const Placed* cardRing = find(card, "expandedLeading/0/0");
    expect(bigTimer && std::get<Text>(bigTimer->item).value == "25:00" && bigTimer->ticking, "card: the time left, ticking");
    expect(cardRing && cardRing->accent == focusRed && cardRing->frame.x < bigTimer->frame.x, "the ring beside it, in the phase's colour");

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
    expect(pausedTile.size() == 2 && pausedTile[1].items[0] == Item{ Text{ "Paused · 2/" + std::to_string(t.pomodoroRounds), TextStyle::Caption } },
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
