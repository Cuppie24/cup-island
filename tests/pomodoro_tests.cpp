// Pomodoro end to end (decision 009): registry + provider + reduce() +
// layout() + hitTest(), with wake-ups driven like the app's timer.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"
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
};

const std::string kTile = std::string("tile/") + PomodoroProvider::kPanelId;
const std::string kStart = "expandedCenter/3/0"; // ▶, under the dormant card's time and its focus and break rows
const std::string kPause = "expandedCenter/4/0"; // the running card's controls: under the dots, above the to-dos
const std::string kStop = "expandedCenter/4/2";

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

// Setting focus and break lengths before starting (decision 044).
void planTests()
{
    const Tokens& t = tokens();
    PomodoroPlan plan = defaultPlan();
    expect(plan.focus == t.pomodoroFocus && plan.rest == t.pomodoroBreak, "plan: tokens.json's lengths by default");
    expect(adjustPlan(plan, true, 1).focus == t.pomodoroFocus + t.pomodoroFocusStep && adjustPlan(plan, false, -1).rest == t.pomodoroBreak - t.pomodoroBreakStep,
           "plan: a step longer or shorter");
    PomodoroPlan shortest = adjustPlan(plan, true, -1000), longest = adjustPlan(plan, false, 1000);
    expect(shortest.focus == t.pomodoroFocusStep && longest.rest == t.pomodoroBreakMax, "plan: kept between one step and the max");
    expect(!canAdjust(shortest, true, -1) && canAdjust(shortest, true, 1) && !canAdjust(longest, false, 1), "plan: − and + dim at the limits");
    PomodoroPlan set{ Ms{ 40 * 60000 }, Ms{ 7 * 60000 } };
    expect(loadPlan(savePlan(set)) == set, "plan: saved and loaded");
    expect(loadPlan("") == plan && loadPlan("garbage") == plan, "plan: unreadable is the default");
    expect(loadPlan("plan1 99999999999 0").focus == t.pomodoroFocusMax && loadPlan("plan1 99999999999 0").rest == t.pomodoroBreakStep,
           "plan: out of range is clamped");

    // On the dormant card: Focus and Break rows with − and +; the length is kept and used.
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
    Scene card = island.scene();
    auto textAt = [&](const Scene& sc, const std::string& key) {
        const Placed* p = find(sc, key);
        return p && std::holds_alternative<Text>(p->item) ? std::get<Text>(p->item).value : std::string();
    };
    expect(textAt(card, "expandedCenter/1/0") == "Focus" && textAt(card, "expandedCenter/1/1") == "25 min", "a Focus row with its length");
    expect(textAt(card, "expandedCenter/2/0") == "Break" && textAt(card, "expandedCenter/2/1") == "5 min", "a Break row with its length");
    const Placed* minus = find(card, "expandedCenter/1/2");
    const Placed* plus = find(card, "expandedCenter/1/3");
    expect(minus && plus && std::get<Button>(minus->item).glyph == Glyph::Minus && std::get<Button>(plus->item).glyph == Glyph::Plus,
           "− and + at the row's end");
    expect(minus->frame.width == t.buttonSize && plus->frame.width == t.buttonSize, "full-size buttons, easy to hit");
    const Placed* value = find(card, "expandedCenter/1/1");
    expect(value->frame.x + value->frame.width <= minus->frame.x && minus->frame.x + minus->frame.width <= plus->frame.x,
           "the length, then − then +, side by side");
    expect(card.size.y <= t.canvas.y, "the card fits the canvas");

    island.press("expandedCenter/1/3");
    island.press("expandedCenter/1/3");
    island.press("expandedCenter/2/2");
    Scene after = island.scene();
    expect(textAt(after, "expandedCenter/0/0") == "35:00" && textAt(after, "expandedCenter/1/1") == "35 min" &&
           textAt(after, "expandedCenter/2/1") == "4 min", "each click a step; the big time follows focus");
    expect(loadPlan(saved).focus == Ms{ 35 * 60000 } && loadPlan(saved).rest == Ms{ 4 * 60000 }, "kept in the store");
    expect(island.press(kStart) && island.registry.nextWake() == Ms{ 35 * 60000 }, "started: focus lasts the planned time");
    island.wake();
    expect(island.focus() && island.focus()->content.label == "Break" && island.registry.nextWake() == island.now + Ms{ 4 * 60000 },
           "and the break too");
    island.registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });
    island.send(HomeOpened{});
    island.send(TileClicked{ PomodoroProvider::kPanelId }); // the break's alert had closed it
    expect(!island.focus() && island.press(kStart) && island.registry.nextWake() == island.now + Ms{ 35 * 60000 },
           "the next session uses the same lengths");

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
    planTests();
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
    expect(island.press(kStart), "the dormant card's start button");
    expect(island.focus() && island.focus()->content.label == "Focus", "start: a focus phase is live");
    expect(island.registry.nextWake() == Ms{ t.pomodoroFocus }, "wakes when focus ends");
    expect(island.state.expandedId == PomodoroProvider::kPanelId, "the live card shows");
    island.send(PillClicked{});
    expect(island.state.expandedId == PomodoroProvider::kPanelId, "running card: a click between its controls keeps it open");

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

    // Expanded: how many phases have passed.
    auto stepsShown = [&] {
        island.send(PillClicked{});
        Scene card = island.scene();
        island.send(EscapePressed{});
        for (const Placed& p : card.elements)
            if (auto* steps = std::get_if<Steps>(&p.item)) return *steps;
        return Steps{ -1, -1 };
    };
    expect(stepsShown() == Steps{ static_cast<int>(2 * t.pomodoroRounds), 0 }, "card: a dot per phase, none done yet");

    // The card: centred, a big countdown, the phase's bar, the dots, then the controls; no ring.
    island.send(PillClicked{});
    Scene card = island.scene();
    island.send(EscapePressed{});
    auto at = [&](const std::string& key) -> const Placed* {
        for (const Placed& p : card.elements)
            if (p.key == key) return &p;
        return nullptr;
    };
    const Placed* bigTimer = at("expandedCenter/0/0");
    const Placed* bar = at("expandedCenter/1/0");
    const Placed* dots = at("expandedCenter/2/0");
    const Placed* caption = at("expandedCenter/3/0");
    const Placed* pauseButton = at(kPause);
    const Placed* field = nullptr;
    for (const Placed& p : card.elements)
        if (std::holds_alternative<TextInput>(p.item)) field = &p;
    expect(bar && std::get<Progress>(bar->item).shape == ProgressShape::Bar && bar->frame.y > bigTimer->frame.y + bigTimer->frame.height - 1,
           "a bar below the countdown");
    expect(bar && dots && dots->frame.y > bar->frame.y + bar->frame.height - 1, "above the dots");
    expect(bar && dots && bar->frame.x == dots->frame.x && bar->frame.width == dots->frame.width, "as wide as the dots");
    expect(bigTimer && std::get<Text>(bigTimer->item).style == TextStyle::Display, "card: a big countdown on top");
    expect(pauseButton && std::holds_alternative<Button>(pauseButton->item) && caption &&
           pauseButton->frame.y > caption->frame.y + caption->frame.height - 1, "the controls below the dots and what they mean");
    expect(pauseButton && field && field->frame.y > pauseButton->frame.y + pauseButton->frame.height - 1, "and above the to-dos");
    expect(std::abs(bigTimer->frame.x + bigTimer->frame.width / 2 - card.size.x / 2) < 0.51f, "the countdown centred");
    expect(dots && std::abs(dots->frame.x + dots->frame.width / 2 - card.size.x / 2) < 0.51f && dots->frame.y > bigTimer->frame.y,
           "the dots centred, below");
    // The card grows to fit its rows (the to-do list); content stays above the tab strip, if any.
    float contentBottom = card.tabs ? card.tabs->tabs[0].frame.y - t.slotGap : card.size.y - t.expandedPadding;
    for (const Placed& p : card.elements) {
        if (auto* progress = std::get_if<Progress>(&p.item)) expect(progress->shape == ProgressShape::Bar, "no ring in the card: " + p.key);
        if (p.key.rfind("tab/", 0) != 0) expect(p.frame.y + p.frame.height <= contentBottom + 0.01f, "fits the card: " + p.key);
    }
    expect(card.size.y <= t.canvas.y, "and the card fits the canvas");

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
    expect(island.focus() && island.state.dialog && island.state.dialog->title == "Stop the session?" &&
           island.state.dialog->message == "You're in round 1 of " + std::to_string(t.pomodoroRounds) + ". The timer starts over next time." &&
           island.state.dialog->destructive, "stop asks first; still running");
    island.send(DialogClosed{}); // Cancel
    expect(island.focus() && !island.state.dialog, "cancelled: still running");
    island.press(kStop);
    Dialog asked = *island.state.dialog;
    island.registry.route({ asked.providerId, asked.activityId, asked.confirmAction, 0 }); // the app: Stop clicked
    island.send(DialogClosed{});
    expect(!island.focus() && !island.registry.nextWake() && island.state.expandedId == PomodoroProvider::kPanelId,
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
    expect(island.focus() && island.state.dialog && island.state.dialog->title == "Stop the session?", "ctrl+w: asks first; still running");
    asked = *island.state.dialog;
    island.registry.route({ asked.providerId, asked.activityId, asked.confirmAction, 0 });
    island.send(DialogClosed{});
    expect(!island.focus() && island.state.expandedId == PomodoroProvider::kPanelId, "ctrl+w: confirmed, stopped; its card still open");
    island.send(HomeOpened{});
    expect(!shownActivity(island.state), "ctrl+w: Home has no activity to close");
    island.send(TileClicked{ PomodoroProvider::kPanelId });

    island.press(kStart);
    island.registry.stop("pomodoro");
    expect(island.state.panels.empty() && !island.focus(), "stopping the provider removes the panel and the session");
    expect(mode(island.state) == Mode::Idle, "and closes Home");

    // A restart (decision 027): the session is saved with the wall clock, and
    // a new run picks it up where it would be now.
    std::string saved;
    Ms wall{ 1'700'000'000'000 };
    auto storage = [&] {
        return ProviderRegistry::Store{
            .load = [&](const std::string&, const std::string& key) { return key == PomodoroProvider::kSessionKey ? saved : ""; },
            .save = [&](const std::string&, const std::string& key, const std::string& s) { if (key == PomodoroProvider::kSessionKey) saved = s; },
            .wallClock = [&] { return wall; } };
    };
    auto run = std::make_unique<Island>(storage());
    run->registry.add(std::make_unique<PomodoroProvider>());
    run->registry.start("pomodoro");
    run->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStart, 0 });
    run->now += Ms{ 60000 };
    wall += Ms{ 60000 };
    run->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kPause, 0 }); // saves: 1 min in, paused
    run.reset(); // the app quits
    wall += Ms{ 3'600'000 };
    auto next = std::make_unique<Island>(storage());
    next->now = Ms{ 500 }; // a new run: the app's own clock started over
    next->registry.add(std::make_unique<PomodoroProvider>());
    next->registry.start("pomodoro");
    const Activity* resumed = next->focus();
    expect(resumed && resumed->content.label == "Focus" && !next->registry.nextWake(), "restarted: the paused session is back, still paused");
    run = std::move(next);
    run->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kPause, 0 }); // resume: running again
    expect(run->registry.nextWake() == run->now + t.pomodoroFocus - Ms{ 60000 }, "an hour closed while paused: the same minute left to go");

    // Running while closed: phases that ended meanwhile pass quietly.
    run.reset();
    wall += t.pomodoroFocus - Ms{ 60000 } + Ms{ 30000 }; // focus ended 30 s ago
    next = std::make_unique<Island>(storage());
    next->registry.add(std::make_unique<PomodoroProvider>());
    next->registry.start("pomodoro");
    expect(next->focus() && next->focus()->content.label == "Break" && !next->state.alert, "focus ended while closed: on the break, no alert");
    expect(next->registry.nextWake() == next->now + t.pomodoroBreak - Ms{ 30000 }, "30 s into it");
    next.reset();
    wall += Ms{ 24 * 3'600'000 };
    next = std::make_unique<Island>(storage());
    next->registry.add(std::make_unique<PomodoroProvider>());
    next->registry.start("pomodoro");
    expect(!next->focus() && next->state.panels.size() == 1, "a day later: the session ended meanwhile, dormant");

    // Stopping forgets it.
    next->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStart, 0 });
    expect(!saved.empty(), "a new session is saved");
    next->registry.route({ "pomodoro", PomodoroProvider::kPanelId, pomodoro_action::kStopConfirmed, 0 });
    expect(saved.empty() && !resumeSession(saved, defaultPlan(), Ms{ 0 }, wall), "stopped: nothing to resume");
    expect(!resumeSession("garbage", defaultPlan(), Ms{ 0 }, wall) && !resumeSession("pomodoro1 7 1 1 0 1000 0", defaultPlan(), Ms{ 0 }, wall), "unreadable: nothing");
    PomodoroSession clockBack = *resumeSession("pomodoro1 0 1 1 1000 60000 5000", defaultPlan(), Ms{ 0 }, Ms{ 1000 });
    expect(clockBack.timeline.position == Ms{ 1000 }, "a clock set back: no time passed");
}

} // namespace cup::test
