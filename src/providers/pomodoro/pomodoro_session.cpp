#include "providers/pomodoro/pomodoro_session.h"

#include "core/tokens.h"

#include <algorithm>
#include <sstream>

namespace cup {

using Phase = PomodoroPhase;

namespace {

// Between one step and the max, on a whole step.
Ms clampToSteps(Ms value, Ms step, Ms max)
{
    if (step <= Ms{ 0 }) return value;
    Ms top = std::max(step, max - max % step);
    return std::clamp(value - value % step, step, top);
}

PomodoroPlan clamped(PomodoroPlan p)
{
    const Tokens& t = tokens();
    p.focus = clampToSteps(p.focus, t.pomodoroFocusStep, t.pomodoroFocusMax);
    p.rest = clampToSteps(p.rest, t.pomodoroBreakStep, t.pomodoroBreakMax);
    return p;
}

} // namespace

PomodoroPlan defaultPlan() { return clamped({ tokens().pomodoroFocus, tokens().pomodoroBreak }); }

PomodoroPlan adjustPlan(PomodoroPlan p, bool focus, int steps)
{
    const Tokens& t = tokens();
    if (focus) p.focus += steps * t.pomodoroFocusStep;
    else p.rest += steps * t.pomodoroBreakStep;
    return clamped(p);
}

bool canAdjust(const PomodoroPlan& plan, bool focus, int steps) { return adjustPlan(plan, focus, steps) != clamped(plan); }

std::string savePlan(const PomodoroPlan& p)
{
    return "plan1 " + std::to_string(p.focus.count()) + " " + std::to_string(p.rest.count()) + "\n";
}

PomodoroPlan loadPlan(const std::string& saved)
{
    std::istringstream in(saved);
    std::string tag;
    long long focus = 0, rest = 0;
    if (!(in >> tag >> focus >> rest) || tag != "plan1") return defaultPlan();
    return clamped({ Ms{ focus }, Ms{ rest } });
}

Ms phaseDuration(Phase phase, const PomodoroPlan& plan)
{
    switch (phase) {
    case Phase::Focus: return plan.focus;
    case Phase::Break: return plan.rest;
    case Phase::LongBreak: return tokens().pomodoroLongBreak;
    }
    return plan.focus;
}

std::optional<PomodoroSession> followingPhase(const PomodoroSession& s, const PomodoroPlan& plan, Ms now)
{
    if (s.phase == Phase::LongBreak) return std::nullopt;
    PomodoroSession next = s;
    if (s.phase == Phase::Focus) {
        next.phase = s.round >= tokens().pomodoroRounds ? Phase::LongBreak : Phase::Break;
    } else {
        next.phase = Phase::Focus;
        next.round = s.round + 1;
    }
    next.timeline = { .duration = phaseDuration(next.phase, plan), .anchoredAt = now, .playing = true };
    return next;
}

// "pomodoro2 <phase> <round> <position ms> <duration ms> <focus skipped>"
std::string saveSession(const PomodoroSession& s, Ms now)
{
    std::ostringstream out;
    out << "pomodoro2 " << static_cast<int>(s.phase) << ' ' << s.round << ' ' << s.timeline.positionAt(now).count() << ' '
        << s.timeline.duration.count() << ' ' << s.focusSkipped << '\n';
    return out.str();
}

std::optional<PomodoroSession> resumeSession(const std::string& saved, Ms now)
{
    std::istringstream in(saved);
    std::string tag;
    int phase = 0, playing = 0, skipped = 0;
    long long round = 0, position = 0, duration = 0, wall = 0;
    if (!(in >> tag)) return std::nullopt;
    if (tag == "pomodoro2") {
        if (!(in >> phase >> round >> position >> duration >> skipped)) return std::nullopt;
    } else if (tag == "pomodoro1") { // decision 027's: playing and the wall clock, no longer needed
        if (!(in >> phase >> round >> playing >> position >> duration >> wall)) return std::nullopt;
    } else {
        return std::nullopt;
    }
    if (phase < 0 || phase > 2 || round < 1 || duration <= 0 || position < 0 || skipped < 0) return std::nullopt;
    return PomodoroSession{ static_cast<Phase>(phase), static_cast<std::size_t>(round),
                            { .position = Ms{ std::min(position, duration) }, .duration = Ms{ duration }, .anchoredAt = now, .playing = false },
                            skipped };
}

} // namespace cup
