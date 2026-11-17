#pragma once
// Pomodoro: the first panel (decision 009). Dormant, its Home widget opens
// a card that starts a session; running, it's an activity that counts down
// focus rounds and breaks. It ends by itself after the long break, or with
// its stop button, once confirmed. The widget only informs (decision 019).
// The card is the time and its controls; its expand button opens the
// timer's window (decision 106) with the rest: the tasks in groups
// (decisions 016, 117), statistics and settings (decision 061).
// Pure: the countdown is host-driven (a Timeline), phase ends come from
// Host::wakeAt, so it builds and tests on any OS. A session survives a
// restart, paused (decisions 027, 061); everything is kept in the host's store (decision 028).

#include "core/provider.h"
#include "providers/pomodoro/pomodoro_card.h"
#include "providers/pomodoro/pomodoro_session.h"
#include "providers/pomodoro/pomodoro_stats.h"
#include "providers/pomodoro/pomodoro_window.h"
#include "providers/pomodoro/task_board.h"

#include <cstddef>
#include <optional>
#include <string>
#include <variant>

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
    // Its store keys (Host::load/save): the tasks (TaskBoard::save; a TodoList's before groups), the session (saveSession; "" = none).
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
    // The tasks (pomodoro_tasks.cpp): a group.* or tasks.* action, or the Undo. True if it was one.
    bool taskAction(const Action& action);
    void countTask(const TaskTarget& target);   // a task ticked or unticked: today's count
    void offerUndo();                           // the host's toast with Undo over the window, for what was just deleted
    PomodoroDay& today();                       // today's statistics, to change (then saveStats)
    PomodoroGlance glance() const;              // today's, now
    void saveStats();
    std::optional<int> fakeRow() const;        // what the settings page's debug row shows; nothing outside Debug builds

    Host* host_ = nullptr;
    std::optional<Session> session_;
    TaskBoard tasks_;
    PomodoroPlan plan_;
    PomodoroStats stats_;
    DaysOff daysOff_{};
    bool windowOpen_ = false;
    // The last task or group deleted, for its toast's Undo (decisions 071, 121); the next change drops it.
    struct DeletedTask {
        std::uint64_t group = 0;
        Todo task;
    };
    std::optional<std::variant<DeletedTask, RemovedGroup>> deleted_;
    std::optional<std::uint64_t> writingIn_; // the group whose "Add task" opened a row being written (decision 132)
    bool debug_ = false;
    int fake_ = 0; // a made-up history shown (fakeStats), never saved; 0: the real one
};

} // namespace cup
