#pragma once
// Pomodoro's window, its full view (decisions 104, 106), pure: what the
// island's card leaves out. Three pages: the timer (the dial, its phase
// dots, the controls, today, the to-dos), statistics and settings. A window
// has no dialogs over it: a question (stop, reset, clear) is asked in place,
// under what it's about, and a deleted to-do's Undo is a row of its own.

#include "core/window_content.h"
#include "providers/pomodoro/pomodoro_card.h"
#include "providers/pomodoro/todo_list.h"

#include <optional>

namespace cup {

inline constexpr const char* kPomodoroTimerPage = "timer";
inline constexpr const char* kPomodoroStatsPage = "stats";
inline constexpr const char* kPomodoroSettingsPage = "settings";

// A question asked in the window, waiting for its answer.
enum class PomodoroQuestion { None, Stop, ResetStats, ClearDone, ClearAll };

struct PomodoroWindowView {
    std::optional<PomodoroSession> session; // none: dormant
    const TodoList* todos = nullptr;
    PomodoroPlan plan = defaultPlan();
    PomodoroGlance glance;
    DaysOff daysOff{};
    PomodoroQuestion question = PomodoroQuestion::None;
    bool undoDelete = false;        // a to-do was just deleted: its Undo row shows
    std::optional<int> fake;        // Debug builds: which made-up history shows (0: the real one); none: no debug row
};

WindowContent pomodoroWindow(const PomodoroWindowView& view);

// The action a question's confirm button sends.
const char* confirmActionOf(PomodoroQuestion question);

} // namespace cup
