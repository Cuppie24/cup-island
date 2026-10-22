#pragma once
// Pomodoro's content, pure (tested on its own): the dormant panel, a running
// session, the stop question, and the timer's own pages: statistics (behind
// its numbers) and settings (the gear), decisions 061 and 063.

#include "core/island.h"
#include "providers/pomodoro/pomodoro_session.h"
#include "providers/pomodoro/pomodoro_stats.h"
#include "providers/pomodoro/todo_list.h"

#include <optional>
#include <string>

namespace cup {

inline constexpr const char* kPomodoroPanelId = "pomodoro.focus"; // the panel and its activity

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
// The pages (decision 061): the chart button opens statistics, the gear settings (decision 066); the back
// arrow (or Esc) leaves them.
inline constexpr const char* kOpenStats = "stats.open";
inline constexpr const char* kOpenSettings = "settings.open";
inline constexpr const char* kBack = "settings.back";
inline constexpr const char* kToggleDayOff = "dayoff."; // + 0 (Monday) .. 6 (Sunday)
inline constexpr const char* kSettingsList = "timersettings"; // the settings page under its title (Row::list): scrolls
inline constexpr const char* kResetStats = "stats.reset";                     // only asks (resetQuestion); its confirm sends:
inline constexpr const char* kResetStatsConfirmed = "stats.reset.confirmed"; // every day but today cleared (decision 071)
inline constexpr const char* kFakeStats = "debug.fakestats"; // Debug builds: the next made-up history (fakeStats)
} // namespace pomodoro_action

// Which page the card shows (decision 061).
enum class PomodoroPage { Timer, Stats, Settings };

// How it's going (decision 061): today's values on the dormant card, and
// with their bests on the statistics page. The bests leave out today and
// the current run: beating them is a record.
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

Panel pomodoroPanel(const TodoList& todos = {}, const PomodoroPlan& plan = defaultPlan(), const PomodoroGlance& glance = {});
Content pomodoroContent(const PomodoroSession& session, const TodoList& todos = {}, const PomodoroPlan& plan = defaultPlan());
// The card on its statistics page instead: now and the bests, a "Record" tag on what beats its best.
Content withStatsPage(Content card, const PomodoroGlance& glance);
// The card on its settings page instead: the plan (dimmed while `running`), the days off. The rest of `card` (its widget, compact) stays.
// `fake`: Debug builds, which made-up history is shown (0: the real one); nothing: no debug row.
Content withSettingsPage(Content card, const PomodoroPlan& plan, bool running, const DaysOff& off, std::optional<int> fake = std::nullopt);
// The question the stop button asks before ending a running session.
Dialog stopQuestion(const PomodoroSession& session);
// ...and the statistics page's Reset statistics, before clearing every day but today.
Dialog resetQuestion();
// Focus and breaks have their own colours (decision 010), in tokens.json.
Color accentOf(PomodoroPhase phase);

} // namespace cup
