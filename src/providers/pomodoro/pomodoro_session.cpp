#include "providers/pomodoro/pomodoro_session.h"

#include "core/tokens.h"

#include <algorithm>
#include <sstream>

namespace cup {

using Phase = PomodoroPhase;

Ms phaseDuration(Phase phase)
{
    const Tokens& t = tokens();
    switch (phase) {
    case Phase::Focus: return t.pomodoroFocus;
    case Phase::Break: return t.pomodoroBreak;
    case Phase::LongBreak: return t.pomodoroLongBreak;
    }
    return t.pomodoroFocus;
}

std::optional<PomodoroSession> followingPhase(const PomodoroSession& s, Ms now)
{
    if (s.phase == Phase::LongBreak) return std::nullopt;
    PomodoroSession next = s;
    if (s.phase == Phase::Focus) {
        next.phase = s.round >= tokens().pomodoroRounds ? Phase::LongBreak : Phase::Break;
    } else {
        next.phase = Phase::Focus;
        next.round = s.round + 1;
    }
    next.timeline = { .duration = phaseDuration(next.phase), .anchoredAt = now, .playing = true };
    return next;
}

// "pomodoro1 <phase> <round> <playing> <position ms> <duration ms> <wall ms>"
std::string saveSession(const PomodoroSession& s, Ms now, Ms wallNow)
{
    std::ostringstream out;
    out << "pomodoro1 " << static_cast<int>(s.phase) << ' ' << s.round << ' ' << (s.timeline.playing ? 1 : 0) << ' '
        << s.timeline.positionAt(now).count() << ' ' << s.timeline.duration.count() << ' ' << wallNow.count() << '\n';
    return out.str();
}

std::optional<PomodoroSession> resumeSession(const std::string& saved, Ms now, Ms wallNow)
{
    std::istringstream in(saved);
    std::string tag;
    int phase = 0, playing = 0;
    long long round = 0, position = 0, duration = 0, wall = 0;
    if (!(in >> tag >> phase >> round >> playing >> position >> duration >> wall) || tag != "pomodoro1") return std::nullopt;
    if (phase < 0 || phase > 2 || round < 1 || duration <= 0 || position < 0) return std::nullopt;

    PomodoroSession s{ static_cast<Phase>(phase), static_cast<std::size_t>(round),
                       { .position = Ms{ std::min(position, duration) }, .duration = Ms{ duration }, .anchoredAt = now, .playing = playing != 0 } };
    if (!s.timeline.playing) return s;
    // It kept running while the app was closed: walk through the phases that ended meanwhile.
    Ms left = s.timeline.position + Ms{ std::max(0LL, wallNow.count() - wall) };
    while (left >= s.timeline.duration) {
        left -= s.timeline.duration;
        auto next = followingPhase(s, now);
        if (!next) return std::nullopt;
        s = *next;
    }
    s.timeline.position = left;
    return s;
}

} // namespace cup
