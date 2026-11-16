#pragma once
// Pomodoro's window, its full view (decisions 104, 106, 117), pure: what the
// island's card leaves out. A rail of three pages: the timer (its time and
// controls across the top, then the tasks, a card per group, as many side by
// side as fit), statistics and, at the rail's foot, settings. A window has
// no dialogs over it: a question (stop, reset) is asked in place, and a
// deleted task's or group's Undo is a row of its own.

#include "core/window_content.h"
#include "providers/pomodoro/pomodoro_card.h"
#include "providers/pomodoro/task_board.h"

#include <optional>
#include <string>

namespace cup {

inline constexpr const char* kPomodoroTimerPage = "timer";
inline constexpr const char* kPomodoroStatsPage = "stats";
inline constexpr const char* kPomodoroSettingsPage = "settings";
inline constexpr const char* kPomodoroUndo = "undo"; // the Undo row's button: the last deleted task or group back

// A question asked in the window, waiting for its answer.
enum class PomodoroQuestion { None, Stop, ResetStats };

struct PomodoroWindowView {
    std::optional<PomodoroSession> session; // none: dormant
    const TaskBoard* tasks = nullptr;
    PomodoroPlan plan = defaultPlan();
    PomodoroGlance glance;
    DaysOff daysOff{};
    PomodoroQuestion question = PomodoroQuestion::None;
    std::string undo;               // what was just deleted ("Task deleted"): its Undo row; "": none
    std::optional<int> fake;        // Debug builds: which made-up history shows (0: the real one); none: no debug row
};

WindowContent pomodoroWindow(const PomodoroWindowView& view);

// The action a question's confirm button sends.
const char* confirmActionOf(PomodoroQuestion question);

} // namespace cup
