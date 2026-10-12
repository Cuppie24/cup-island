#pragma once
// Pomodoro: the first panel (decision 009). Dormant, its Home widget opens
// a card that starts a session; running, it's an activity that counts down
// focus rounds and breaks. It ends by itself after the long break, or with
// its card's stop button, once confirmed. The widget only informs (decision 019).
// Pure: the countdown is host-driven (a Timeline), phase ends come from
// Host::wakeAt, so it builds and tests on any OS. It keeps a to-do list
// under the timer (decision 016), and a running session survives a restart
// (decision 027); saving both is handed in, so it stays out of the pure code.

#include "core/provider.h"
#include "providers/pomodoro/pomodoro_session.h"
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
inline constexpr const char* kStop = "stop";                   // only asks (stopQuestion); the dialog's confirm sends:
inline constexpr const char* kStopConfirmed = "stop.confirmed";
} // namespace pomodoro_action

// Where Pomodoro keeps things between runs, as text: the to-do list
// (TodoList::save), and the session (saveSession: "" when there's none),
// with the wall clock it's stamped with. Any part may be missing (tests).
struct PomodoroStorage {
    std::function<std::string()> loadTodos;
    std::function<void(const std::string&)> saveTodos;
    std::function<std::string()> loadSession;
    std::function<void(const std::string&)> saveSession;
    std::function<Ms()> wallClock; // ms since the epoch
};

class PomodoroProvider : public Provider {
public:
    explicit PomodoroProvider(PomodoroStorage storage = {}) : storage_(std::move(storage)) {}

    const char* id() const override { return "pomodoro"; }
    void start(Host& host) override; // publishes the dormant panel, and resumes a saved session
    void stop() override;            // ends a running session, removes the panel
    void onAction(const Action& action) override;
    void onWake() override;          // the current phase's time is up

    static constexpr const char* kPanelId = "pomodoro.focus"; // the panel and its activity

    using Phase = PomodoroPhase;
    using Session = PomodoroSession;

private:
    void begin(Phase phase, std::size_t round); // starts a phase now
    void advance(bool alert);                   // to the next phase, or the end
    void finish();                              // no session any more: dormant panel
    void publish();                             // activity content + the wake-up for the phase end; saves the session
    void showTodos(bool save = true);           // the list (or its scroll) changed: save it, show it on whichever card is up

    Host* host_ = nullptr;
    std::optional<Session> session_;
    TodoList todos_;
    std::size_t todoScroll_ = 0; // the first to-do in the card's window (decision 020)
    PomodoroStorage storage_;
};

// Content, pure (tested on its own): the dormant panel, and a running session.
// `scroll`: the first to-do in the list's window.
Panel pomodoroPanel(const TodoList& todos = {}, std::size_t scroll = 0);
Content pomodoroContent(const PomodoroProvider::Session& session, const TodoList& todos = {}, std::size_t scroll = 0);
// The question the stop button asks before ending a running session.
Dialog stopQuestion(const PomodoroProvider::Session& session);

} // namespace cup
