#pragma once
// Pomodoro: the first panel (decision 009). Dormant, its Home tile starts a
// session; running, it's an activity that counts down focus rounds and
// breaks. It ends by itself after the long break, or with its stop button.
// Pure: the countdown is host-driven (a Timeline), phase ends come from
// Host::wakeAt, so it builds and tests on any OS. It keeps a to-do list
// under the timer (decision 016); saving it is handed in, so it stays out of
// the pure code.

#include "core/provider.h"
#include "providers/pomodoro/todo_list.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace cup {

namespace pomodoro_action {
inline constexpr const char* kStart = "start";
inline constexpr const char* kPause = "pause"; // toggles pause / resume
inline constexpr const char* kSkip = "skip";   // ends the current phase early, without an alert
inline constexpr const char* kStop = "stop";
} // namespace pomodoro_action

// Where the to-do list lives between runs: the whole list as text (TodoList::save).
struct TodoStorage {
    std::function<std::string()> load;
    std::function<void(const std::string&)> save;
};

class PomodoroProvider : public Provider {
public:
    explicit PomodoroProvider(TodoStorage storage = {}) : storage_(std::move(storage)) {}

    const char* id() const override { return "pomodoro"; }
    void start(Host& host) override; // publishes the dormant panel
    void stop() override;            // ends a running session, removes the panel
    void onAction(const Action& action) override;
    void onWake() override;          // the current phase's time is up

    static constexpr const char* kPanelId = "pomodoro.focus"; // the panel and its activity

    enum class Phase { Focus, Break, LongBreak };
    struct Session {
        Phase phase = Phase::Focus;
        std::size_t round = 1; // 1-based focus round
        Timeline timeline;     // the phase: duration, position, playing = not paused
    };

private:
    void begin(Phase phase, std::size_t round); // starts a phase now
    void advance(bool alert);                   // to the next phase, or the end
    void finish();                              // no session any more: dormant panel
    void publish();                             // activity content + the wake-up for the phase end
    void showTodos();                           // the list changed: save it, show it on whichever card is up

    Host* host_ = nullptr;
    std::optional<Session> session_;
    TodoList todos_;
    TodoStorage storage_;
};

// Content, pure (tested on its own): the dormant panel, and a running session.
Panel pomodoroPanel(const TodoList& todos = {});
Content pomodoroContent(const PomodoroProvider::Session& session, const TodoList& todos = {});

} // namespace cup
