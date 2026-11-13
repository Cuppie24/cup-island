#pragma once
// Pomodoro: the first panel (decision 009). Dormant, its Home widget opens
// a card that starts a session; running, it's an activity that counts down
// focus rounds and breaks. It ends by itself after the long break, or with
// its stop button, once confirmed. The widget only informs (decision 019).
// The card is only the dial and its controls; its expand button opens the
// timer's window (decision 106) with the rest: the phase dots, today, the
// to-dos (decision 016), statistics and settings (decision 061).
// Pure: the countdown is host-driven (a Timeline), phase ends come from
// Host::wakeAt, so it builds and tests on any OS. A session survives a
// restart, paused (decisions 027, 061); everything is kept in the host's store (decision 028).

#include "core/provider.h"
#include "providers/pomodoro/pomodoro_card.h"
#include "providers/pomodoro/pomodoro_session.h"
#include "providers/pomodoro/pomodoro_stats.h"
#include "providers/pomodoro/pomodoro_window.h"
#include "providers/pomodoro/todo_list.h"

#include <cstddef>
#include <optional>
#include <string>

namespace cup {

class PomodoroProvider : public Provider {
public:
    // `debug`: Debug builds, a "Fake statistics" row on the settings page (decision 061).
    explicit PomodoroProvider(bool debug = false) : debug_(debug) {}
    const char* id() const override { return "pomodoro"; }
    void start(Host& host) override; // publishes the dormant panel, and resumes a saved session
    void stop() override;            // keeps a running session to resume paused, removes the panel, closes the window
    void onAction(const Action& action) override; // the card's and the window's
    void onWake() override;          // the current phase's time is up
    void onCloseRequested(const std::string& activityId) override; // Ctrl+W: like the stop button, asks first
    void onCardShown(const std::string& activityId) override;      // a new day: today's statistics follow
    void onWindowClosed(const std::string& windowId) override;     // a question waiting in it is dropped

    static constexpr const char* kPanelId = kPomodoroPanelId; // the panel and its activity
    static constexpr const char* kWindowId = kPomodoroWindowId;
    // Its store keys (Host::load/save): the list (TodoList::save), the session (saveSession; "" = none).
    static constexpr const char* kTodosKey = "todos";
    static constexpr const char* kSessionKey = "session";
    static constexpr const char* kPlanKey = "plan"; // focus and break lengths (savePlan)
    static constexpr const char* kStatsKey = "stats";     // saveStats
    static constexpr const char* kDaysOffKey = "daysoff"; // saveDaysOff

    using Phase = PomodoroPhase;
    using Session = PomodoroSession;

private:
    void begin(Phase phase, std::size_t round); // starts a phase now
    void advance(bool skipped);                 // to the next phase (an alert unless skipped), or the end
    void finish();                              // no session any more: dormant panel
    void publish();                             // activity content + the wake-up for the phase end; saves the session
    void showDormant();                         // the dormant panel
    void showAll();                             // the card that's up (the session's, or the dormant one) and the window
    void showWindow();                          // the window's content, if it's open
    void openWindow();                          // from the card's expand button: opened, or brought forward
    void ask(PomodoroQuestion question);        // a question in the window, or none (answered, Cancel, another page)
    bool todoAction(const Action& action, bool fromWindow); // a todo.* action: true if it was one
    void countTodo(const std::string& actionId); // a to-do ticked or unticked: today's count
    PomodoroDay& today();                       // today's statistics, to change (then saveStats)
    PomodoroGlance glance() const;              // today's, now
    void saveStats();
    std::optional<int> fakeRow() const;        // what the settings page's debug row shows; nothing outside Debug builds

    Host* host_ = nullptr;
    std::optional<Session> session_;
    TodoList todos_;
    PomodoroPlan plan_;
    PomodoroStats stats_;
    DaysOff daysOff_{};
    bool windowOpen_ = false;
    PomodoroQuestion question_ = PomodoroQuestion::None; // asked in the window, waiting
    std::optional<Todo> deleted_; // the last to-do deleted, for its Undo (decision 071)
    bool undoInWindow_ = false;   // ...deleted in the window: its Undo row shows there
    bool debug_ = false;
    int fake_ = 0; // a made-up history shown (fakeStats), never saved; 0: the real one
};

} // namespace cup
