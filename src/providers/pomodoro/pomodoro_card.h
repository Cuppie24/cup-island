#pragma once
// Pomodoro's content, pure (tested on its own): the dormant panel, a running
// session, the stop question, and the timer's own settings page behind the
// gear in the card's top-right corner (decision 061).

#include "core/island.h"
#include "providers/pomodoro/pomodoro_session.h"
#include "providers/pomodoro/pomodoro_stats.h"
#include "providers/pomodoro/todo_list.h"

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
// The settings page (decision 061): the gear opens it, the back arrow (or Esc) leaves it.
inline constexpr const char* kOpenSettings = "settings.open";
inline constexpr const char* kBack = "settings.back";
inline constexpr const char* kToggleDayOff = "dayoff."; // + 0 (Monday) .. 6 (Sunday)
} // namespace pomodoro_action

// What the dormant card says about how it's going (decision 061).
struct PomodoroGlance {
    int finishedToday = 0;
    int bestDay = 0;
    int daysInARow = 0;
    Ms focusToday{};
    bool operator==(const PomodoroGlance&) const = default;
};
PomodoroGlance glanceOf(const PomodoroStats& stats, const DaysOff& off, long long today);

Panel pomodoroPanel(const TodoList& todos = {}, const PomodoroPlan& plan = defaultPlan(), const PomodoroGlance& glance = {});
Content pomodoroContent(const PomodoroSession& session, const TodoList& todos = {});
// The card on its settings page instead: the days off. The rest of `card` (its widget, compact) stays.
Content withSettingsPage(Content card, const DaysOff& off);
// The question the stop button asks before ending a running session.
Dialog stopQuestion(const PomodoroSession& session);
// Focus and breaks have their own colours (decision 010), in tokens.json.
Color accentOf(PomodoroPhase phase);

} // namespace cup
