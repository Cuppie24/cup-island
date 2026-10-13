#pragma once
// A Pomodoro session's phases, and how it outlives the app (decision 027).
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

// One line: where the session is at `now`, stamped with the wall clock
// (`wallNow`, ms since the epoch). The app's own clock starts over each run.
std::string saveSession(const PomodoroSession& s, Ms now, Ms wallNow);

// The session as it stands now: a running one has gone on while the app was
// closed (phases that ended meanwhile pass quietly, no alerts); a paused one
// is where it was. Nothing if none was saved, the text is unreadable, or the
// session ended meanwhile. A clock set back counts as no time passed.
std::optional<PomodoroSession> resumeSession(const std::string& saved, const PomodoroPlan& plan, Ms now, Ms wallNow);

} // namespace cup
