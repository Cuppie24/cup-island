#pragma once
// Pomodoro's window, its full view (decisions 104, 106, 117), pure: what the
// island's card leaves out. A rail of three pages: the timer (its time and
// controls across the top, then the tasks, a card per group, as many side by
// side as fit), statistics and, at the rail's foot, settings. Its questions
// (stop, reset, deleting a group) are the host's, over the window (decisions
// 122, 126, 128); a deleted task's or group's Undo is the host's toast (decision 121).

#include "core/window_content.h"
#include "providers/pomodoro/pomodoro_card.h"
#include "providers/pomodoro/task_board.h"

#include <optional>
#include <string>

namespace cup {

inline constexpr const char* kPomodoroTimerPage = "timer";
inline constexpr const char* kPomodoroStatsPage = "stats";
inline constexpr const char* kPomodoroSettingsPage = "settings";
inline constexpr const char* kPomodoroUndo = "undo"; // the delete toast's Undo: the last deleted task or group back

struct PomodoroWindowView {
    std::optional<PomodoroSession> session; // none: dormant
    const TaskBoard* tasks = nullptr;
    PomodoroPlan plan = defaultPlan();
    PomodoroGlance glance;
    DaysOff daysOff{};
    std::optional<int> fake;        // Debug builds: which made-up history shows (0: the real one); none: no debug row
    std::optional<std::uint64_t> writingIn; // the group with a task row being written under its "Add task" (decision 132)
};

WindowContent pomodoroWindow(const PomodoroWindowView& view);

} // namespace cup
