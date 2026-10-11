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
    ProviderRegistry registry{ [this](const Event& e, const char*) { state = reduce(state, e, now); }, [this] { return now; } };

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

const std::string kTileButton = std::string("tile/") + PomodoroProvider::kPanelId + "/0/2"; // start, then stop

} // namespace

void runPomodoroTests()
{
    const Tokens& t = tokens();
    Island island;
    island.registry.add(std::make_unique<PomodoroProvider>());
    island.registry.start("pomodoro");
    expect(island.state.panels.size() == 1 && island.state.panels[0].providerId == "pomodoro", "starting publishes the panel");
    expect(mode(island.state) == Mode::Idle && !island.registry.nextWake(), "dormant: idle, nothing scheduled");

    // Start from Home's tile.
    island.send(PillClicked{});
    expect(island.state.homeOpen, "idle click opens Home");
    expect(island.press(kTileButton), "the dormant tile's start button is clickable");
    expect(island.focus() && island.focus()->content.label == "Focus", "start: a focus phase is live");
    expect(island.registry.nextWake() == Ms{ t.pomodoroFocus }, "wakes when focus ends");
    expect(island.state.homeOpen && island.scene().tiles.size() == 1, "Home stays open, the tile in place");
    const Placed* left = nullptr;
    Scene home = island.scene();
    for (const Placed& p : home.elements)
        if (p.key == std::string("tile/") + PomodoroProvider::kPanelId + "/0/1") left = &p;
    expect(left && std::get<Text>(left->item).value == "25:00", "the tile counts down");

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

    // The card: centred, controls on top, a big countdown, the phase's bar, then the dots; no ring.
    island.send(PillClicked{});
    Scene card = island.scene();
    island.send(EscapePressed{});
    auto at = [&](const std::string& key) -> const Placed* {
        for (const Placed& p : card.elements)
            if (p.key == key) return &p;
        return nullptr;
    };
    const Placed* pauseButton = at("expandedCenter/0/0");
    const Placed* bigTimer = at("expandedCenter/1/0");
    const Placed* bar = at("expandedCenter/2/0");
    const Placed* dots = at("expandedCenter/3/0");
    expect(bar && std::get<Progress>(bar->item).shape == ProgressShape::Bar && bar->frame.y > bigTimer->frame.y + bigTimer->frame.height - 1,
           "a bar below the countdown");
    expect(bar && dots && dots->frame.y > bar->frame.y + bar->frame.height - 1, "above the dots");
    expect(bar && dots && bar->frame.x == dots->frame.x && bar->frame.width == dots->frame.width, "as wide as the dots");
    expect(pauseButton && std::holds_alternative<Button>(pauseButton->item), "card: controls on top");
    expect(bigTimer && std::get<Text>(bigTimer->item).style == TextStyle::Display && pauseButton->frame.y < bigTimer->frame.y,
           "card: a big countdown below them");
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

    // Manual exit: the stop button on the tile.
    island.send(PillClicked{});
    island.press(kTileButton);
    expect(island.focus() != nullptr, "started again");
    expect(island.press(kTileButton), "the running tile's stop button is clickable");
    expect(!island.focus() && !island.registry.nextWake() && island.state.homeOpen, "stop: dormant, still on Home");

    island.press(kTileButton);
    island.registry.stop("pomodoro");
    expect(island.state.panels.empty() && !island.focus(), "stopping the provider removes the panel and the session");
    expect(mode(island.state) == Mode::Idle, "and closes Home");
}

} // namespace cup::test
