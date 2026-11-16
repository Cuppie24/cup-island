#pragma once
// Pomodoro on the island, pure (tested on its own): the dormant panel and a
// running session's card, each the time, its phases and its controls, with
// buttons that open the timer's window, where everything else is
// (pomodoro_window.h, decisions 106, 107); the stop question.

#include "core/island.h"
#include "providers/pomodoro/pomodoro_session.h"
#include "providers/pomodoro/pomodoro_stats.h"

#include <string>

namespace cup {

inline constexpr const char* kPomodoroPanelId = "pomodoro.focus"; // the panel and its activity
inline constexpr const char* kPomodoroWindowId = "timer";         // its window (decision 106)
// The card's button into the window (decision 107): hidden while the window
// wasn't finished, shown since its redesign (decision 117).
inline constexpr bool kPomodoroWindowReady = true;

namespace pomodoro_action {
inline constexpr const char* kStart = "start";
inline constexpr const char* kPause = "pause"; // toggles pause / resume
inline constexpr const char* kSkip = "skip";   // ends the current phase early, without an alert
inline constexpr const char* kStop = "stop";                   // only asks (stopQuestion, or in the window); the confirm sends:
inline constexpr const char* kStopConfirmed = "stop.confirmed";
inline constexpr const char* kOpenWindow = "window.open"; // the card's expand button: the timer's window (decision 106)
inline constexpr const char* kCancel = "confirm.cancel"; // the window's question answered No
// The settings page's − and + (decision 044): a step shorter or longer.
inline constexpr const char* kFocusLess = "plan.focus.less";
inline constexpr const char* kFocusMore = "plan.focus.more";
inline constexpr const char* kBreakLess = "plan.break.less";
inline constexpr const char* kBreakMore = "plan.break.more";
inline constexpr const char* kToggleDayOff = "dayoff."; // + 0 (Monday) .. 6 (Sunday)
inline constexpr const char* kResetStats = "stats.reset";                     // only asks; its confirm sends:
inline constexpr const char* kResetStatsConfirmed = "stats.reset.confirmed"; // every day but today cleared (decision 071)
inline constexpr const char* kFakeStats = "debug.fakestats"; // Debug builds: the next made-up history (fakeStats)
} // namespace pomodoro_action

// How it's going (decision 061): today's values, and with their bests on the
// statistics page. The bests leave out today and the current run: beating them is a record.
struct PomodoroGlance {
    int finishedToday = 0;
    int bestDay = 0;
    int daysInARow = 0;
    int bestRun = 0;
    Ms focusToday{};
    Ms bestFocus{};
    bool operator==(const PomodoroGlance&) const = default;
};
PomodoroGlance glanceOf(const PomodoroStats& stats, const DaysOff& off, long long today);

// Dormant: the focus time, the steps to come and ▶. Its widget: the planned focus and the rounds.
Panel pomodoroPanel(const PomodoroPlan& plan = defaultPlan());
// Running: the time, the steps and stop, pause, skip (decision 107); compact, minimal and the widget count down.
Content pomodoroContent(const PomodoroSession& session, const PomodoroPlan& plan = defaultPlan());

// Shared with the window: the time in a ring that fills as it goes, its label under it (decision 065).
Row dialRow(const Timeline& timeline, const std::string& label, const PomodoroPlan& plan);
// A phase's name, and "2 of 8 done" (paused: "Paused · ..."), under the dial's dots.
std::string phaseName(PomodoroPhase phase);
std::string progressCaption(const PomodoroSession& session);
// Beside the phase's name: "round 2 of 4", "last one"; paused, "paused · " first.
std::string sessionDetail(const PomodoroSession& session);
int phaseCount();
int phasesDone(const PomodoroSession& session);
// The session's phases as steps, `done` of them done (decision 107): a
// focus a long pill, a break a dot, the long break a short pill. The current
// one in the phase's colour only while the timer runs (decision 122).
Steps phaseSteps(int done, bool running = true);

// The question the stop button asks before ending a running session.
Dialog stopQuestion(const PomodoroSession& session);
// ...and the statistics page's Reset statistics, before clearing every day but today.
Dialog resetQuestion();
// Focus and breaks have their own colours (decision 010), in tokens.json.
Color accentOf(PomodoroPhase phase);

} // namespace cup
