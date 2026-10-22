#pragma once
// A Pomodoro session's phases, and how it outlives the app (decisions 027, 061).
// Pure: the wall clock is passed in, so a restart's catch-up is tested on Linux.

#include "core/content.h"

#include <cstddef>
#include <optional>
#include <string>

namespace cup {

enum class PomodoroPhase { Focus, Break, LongBreak };

struct PomodoroSession {
    PomodoroPhase phase = PomodoroPhase::Focus;
    std::size_t round = 1; // 1-based focus round
    Timeline timeline;     // the phase: duration, position, playing = not paused
    int focusSkipped = 0;  // focus phases ended with Skip: too many and it doesn't count as finished (decision 061)
};

// How long focus and breaks last: the user's, set on the dormant card
// (decision 044). The long break and the rounds stay in tokens.json.
struct PomodoroPlan {
    Ms focus{};
    Ms rest{}; // a break ("break" is a keyword)
    bool operator==(const PomodoroPlan&) const = default;
};

PomodoroPlan defaultPlan(); // tokens.json's pomodoroFocusMs and pomodoroBreakMs
// Focus (or the break, `focus` false) `steps` steps longer (negative: shorter),
// kept between one step and its max (tokens.json).
PomodoroPlan adjustPlan(PomodoroPlan plan, bool focus, int steps);
bool canAdjust(const PomodoroPlan& plan, bool focus, int steps); // the − or + would change something
// One line, "plan1 <focus ms> <break ms>". Unreadable or missing: the default; out of range: clamped.
std::string savePlan(const PomodoroPlan& plan);
PomodoroPlan loadPlan(const std::string& saved);

Ms phaseDuration(PomodoroPhase phase, const PomodoroPlan& plan);

// What follows `s`, started at `now`: focus -> break -> focus ... -> long
// break after the last round. Nothing after the long break: the session is done.
std::optional<PomodoroSession> followingPhase(const PomodoroSession& s, const PomodoroPlan& plan, Ms now);

// One line: the phase, the round, how far into it at `now`, and the skips.
std::string saveSession(const PomodoroSession& s, Ms now);

// The saved session, paused where it was (decision 061: no time passes while
// the app is closed; the user picks up when they're back). Nothing if none
// was saved or the text is unreadable. Reads decision 027's older lines too.
std::optional<PomodoroSession> resumeSession(const std::string& saved, Ms now);

} // namespace cup
