#pragma once
// Pomodoro: the first panel (decision 009). Dormant, its Home widget opens
// a card that starts a session; running, it's an activity that counts down
// focus rounds and breaks. It ends by itself after the long break, or with
// its card's stop button, once confirmed. The widget only informs (decision 019).
// Pure: the countdown is host-driven (a Timeline), phase ends come from
// Host::wakeAt, so it builds and tests on any OS. It keeps a to-do list
// under the timer (decision 016), a session survives a restart, paused
// (decisions 027, 061), and it keeps statistics and days off (decision 061);
// all in the host's store (decision 028).

#include "core/provider.h"
#include "providers/pomodoro/pomodoro_card.h"
#include "providers/pomodoro/pomodoro_session.h"
#include "providers/pomodoro/pomodoro_stats.h"
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
    void stop() override;            // keeps a running session to resume paused, removes the panel
    void onAction(const Action& action) override;
    void onWake() override;          // the current phase's time is up
    void onCloseRequested(const std::string& activityId) override; // Ctrl+W: like the stop button, asks first
    void onCardClosed(const std::string& activityId) override;     // back from the settings page
    void onCardShown(const std::string& activityId) override;      // a new day: the dormant card's "today" follows

    static constexpr const char* kPanelId = kPomodoroPanelId; // the panel and its activity
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
    void showDormant();                         // the dormant panel, with today's statistics
    void showCards();                           // whichever card is up: the session's, or the dormant one
    void countTodo(const std::string& actionId); // a to-do ticked or unticked: today's count
    PomodoroDay& today();                       // today's statistics, to change (then saveStats)
    void saveStats();
    std::optional<int> fakeRow() const;        // what the settings page's debug row shows; nothing outside Debug builds

    Host* host_ = nullptr;
    std::optional<Session> session_;
    TodoList todos_;
    PomodoroPlan plan_;
    PomodoroStats stats_;
    DaysOff daysOff_{};
    bool settingsOpen_ = false; // the card shows its settings page
    PomodoroGlance shownGlance_; // what the dormant card says now
    bool debug_ = false;
    int fake_ = 0; // a made-up history shown (fakeStats), never saved; 0: the real one
};

} // namespace cup
