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

Ms phaseDuration(PomodoroPhase phase); // from tokens.json

// What follows `s`, started at `now`: focus -> break -> focus ... -> long
// break after the last round. Nothing after the long break: the session is done.
std::optional<PomodoroSession> followingPhase(const PomodoroSession& s, Ms now);

// One line: where the session is at `now`, stamped with the wall clock
// (`wallNow`, ms since the epoch). The app's own clock starts over each run.
std::string saveSession(const PomodoroSession& s, Ms now, Ms wallNow);

// The session as it stands now: a running one has gone on while the app was
// closed (phases that ended meanwhile pass quietly, no alerts); a paused one
// is where it was. Nothing if none was saved, the text is unreadable, or the
// session ended meanwhile. A clock set back counts as no time passed.
std::optional<PomodoroSession> resumeSession(const std::string& saved, Ms now, Ms wallNow);

} // namespace cup
