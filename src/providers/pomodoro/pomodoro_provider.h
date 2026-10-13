#pragma once
// Pomodoro: the first panel (decision 009). Dormant, its Home widget opens
// a card that starts a session; running, it's an activity that counts down
// focus rounds and breaks. It ends by itself after the long break, or with
// its card's stop button, once confirmed. The widget only informs (decision 019).
// Pure: the countdown is host-driven (a Timeline), phase ends come from
// Host::wakeAt, so it builds and tests on any OS. It keeps a to-do list
// under the timer (decision 016), and a running session survives a restart
// (decision 027); both are kept in the host's store (decision 028).

#include "core/provider.h"
#include "providers/pomodoro/pomodoro_session.h"
#include "providers/pomodoro/todo_list.h"

#include <cstddef>
#include <optional>
#include <string>

namespace cup {

namespace pomodoro_action {
inline constexpr const char* kStart = "start";
inline constexpr const char* kPause = "pause"; // toggles pause / resume
inline constexpr const char* kSkip = "skip";   // ends the current phase early, without an alert
inline constexpr const char* kStop = "stop";                   // only asks (stopQuestion); the dialog's confirm sends:
inline constexpr const char* kStopConfirmed = "stop.confirmed";
// The dormant card's − and + (decision 044): a step shorter or longer.
inline constexpr const char* kFocusLess = "plan.focus.less";
inline constexpr const char* kFocusMore = "plan.focus.more";
inline constexpr const char* kBreakLess = "plan.break.less";
inline constexpr const char* kBreakMore = "plan.break.more";
} // namespace pomodoro_action

class PomodoroProvider : public Provider {
public:
    const char* id() const override { return "pomodoro"; }
    void start(Host& host) override; // publishes the dormant panel, and resumes a saved session
    void stop() override;            // ends a running session, removes the panel
    void onAction(const Action& action) override;
    void onWake() override;          // the current phase's time is up
    void onCloseRequested(const std::string& activityId) override; // Ctrl+W: like the stop button, asks first

    static constexpr const char* kPanelId = "pomodoro.focus"; // the panel and its activity
    // Its store keys (Host::load/save): the list (TodoList::save), the session (saveSession; "" = none).
    static constexpr const char* kTodosKey = "todos";
    static constexpr const char* kSessionKey = "session";
    static constexpr const char* kPlanKey = "plan"; // focus and break lengths (savePlan)

    using Phase = PomodoroPhase;
    using Session = PomodoroSession;

private:
    void begin(Phase phase, std::size_t round); // starts a phase now
    void advance(bool alert);                   // to the next phase, or the end
    void finish();                              // no session any more: dormant panel
    void publish();                             // activity content + the wake-up for the phase end; saves the session
    void showTodos();                           // the list changed: save it, show it on whichever card is up

    Host* host_ = nullptr;
    std::optional<Session> session_;
    TodoList todos_;
    PomodoroPlan plan_;
};

// Content, pure (tested on its own): the dormant panel, and a running session.
Panel pomodoroPanel(const TodoList& todos = {}, const PomodoroPlan& plan = defaultPlan());
Content pomodoroContent(const PomodoroProvider::Session& session, const TodoList& todos = {});
// The question the stop button asks before ending a running session.
Dialog stopQuestion(const PomodoroProvider::Session& session);

} // namespace cup
