#include "providers/pomodoro/pomodoro_card.h"

#include "core/tokens.h"

#include <algorithm>
#include <string>

namespace cup {
namespace {

using Phase = PomodoroPhase;
namespace act = pomodoro_action;

// The card (decision 107): a header, the phase's dot in its colour, its
// name and the round, the window's button at the right (statistics and
// settings are its pages); the time left, big and heavy, centred; the session's phases as
// steps under it; then the controls, ▶/⏸ the filled circle between stop and
// skip. Stacked and centred: the time is what it's about. It fits the
// height every card and Home share, so switching tabs doesn't resize the island.
Content timerCard(Content c, const Timeline& timeline, const Steps& steps, const std::string& name, const std::string& detail,
                  std::vector<Item> controls)
{
    c.card = CardLayout::Centred;
    c.keepOpen = true;      // a click between the controls doesn't close it (Content::keepOpen)
    c.noIdleTimeout = true; // nor does being left alone: a timer kept in view (decision 062)
    Row header{ .leading = { Steps{ .count = 1 }, Text{ name, TextStyle::Title }, Text{ detail, TextStyle::Caption } } };
    if (kPomodoroWindowReady) // hidden while the window isn't finished
        header.trailing = { Button{ .glyph = Glyph::Expand, .actionId = act::kOpenWindow, .compact = true, .secondary = true } };
    Row time = row({ TimeLabel{ .timeline = timeline, .countdown = true, .style = TextStyle::Hero } }, Align::Center);
    Row phases{ .items = { steps }, .align = Align::Center, .apart = true };
    Row buttons{ .items = std::move(controls), .align = Align::Center, .loose = true, .apart = true }; // well-separated targets
    c.expandedCenter = { header, time, phases, buttons };
    return c;
}

std::string roundsOf(int round) { return "round " + std::to_string(round) + " of " + std::to_string(tokens().pomodoroRounds); }

} // namespace

std::string phaseName(Phase phase)
{
    switch (phase) {
    case Phase::Focus: return "Focus";
    case Phase::Break: return "Break";
    case Phase::LongBreak: return "Long break";
    }
    return "";
}

// Every phase of a session, in order: focus, break, ... focus, long break.
int phaseCount() { return 2 * static_cast<int>(tokens().pomodoroRounds); }

int phasesDone(const PomodoroSession& s)
{
    int round = static_cast<int>(s.round);
    switch (s.phase) {
    case Phase::Focus: return 2 * (round - 1);
    case Phase::Break: return 2 * round - 1;
    case Phase::LongBreak: return phaseCount() - 1;
    }
    return 0;
}

std::string progressCaption(const PomodoroSession& s)
{
    std::string caption = std::to_string(phasesDone(s)) + " of " + std::to_string(phaseCount()) + " done"; // the dial says the phase
    return s.timeline.playing ? caption : "Paused · " + caption;
}

Steps phaseSteps(int done)
{
    Steps steps{ .count = phaseCount(), .done = done };
    for (int i = 0; i < steps.count; ++i) // focus, break, ... focus, long break
        steps.lengths.push_back(i == steps.count - 1 ? StepLength::Short : i % 2 == 0 ? StepLength::Long : StepLength::Dot);
    return steps;
}

// One size whatever the phase (sized for the plan's longest).
Row dialRow(const Timeline& timeline, const std::string& label, const PomodoroPlan& plan)
{
    Ms longest = std::max({ plan.focus, plan.rest, tokens().pomodoroLongBreak });
    return row({ Dial{ .timeline = timeline, .label = label, .sizedFor = longest } }, Align::Center);
}

Color accentOf(Phase phase) { return phase == Phase::Focus ? tokens().pomodoroFocusColor : tokens().pomodoroBreakColor; }

PomodoroGlance glanceOf(const PomodoroStats& stats, const DaysOff& off, long long today)
{
    PomodoroDay day = countsOf(stats, today);
    Streaks runs = streaksOf(stats, off, today);
    Ms bestFocus{};
    for (const PomodoroDay& d : stats.days)
        if (d.day != today) bestFocus = std::max(bestFocus, d.focus);
    return { .finishedToday = day.finished, .bestDay = bestDay(stats, today), .daysInARow = runs.current, .bestRun = runs.bestBefore,
             .focusToday = day.focus, .bestFocus = bestFocus };
}

Dialog stopQuestion(const PomodoroSession& s)
{
    std::string rounds = std::to_string(tokens().pomodoroRounds);
    std::string where = s.phase == Phase::LongBreak ? "You're on the long break."
                                                    : "You're in round " + std::to_string(s.round) + " of " + rounds + ".";
    return { .activityId = kPomodoroPanelId, .title = "Stop the session?",
             .message = where + " The timer starts over next time.", .confirmLabel = "Stop",
             .confirmAction = act::kStopConfirmed, .destructive = true };
}

Dialog resetQuestion()
{
    return { .activityId = kPomodoroPanelId, .title = "Reset statistics?",
             .message = "Every day before today is cleared: records and days in a row start over. Today's count stays.",
             .confirmLabel = "Reset", .confirmAction = act::kResetStatsConfirmed, .destructive = true };
}

// Dormant (decisions 106, 107): the planned focus, the steps to come, ▶ under them.
// The to-dos, today, the statistics and settings are the window's. The
// widget is laid out like the running one: the timer icon where the ring
// goes, the focus time, the rounds.
Panel pomodoroPanel(const PomodoroPlan& plan)
{
    const Tokens& t = tokens();
    std::string rounds = "Focus · " + std::to_string(t.pomodoroRounds) + " rounds";
    Content content{
        .label = "Focus",
        .accent = accentOf(Phase::Focus),
        .tileLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .tile = { row({ Text{ formatDuration(plan.focus), TextStyle::Title } }), row({ Text{ rounds, TextStyle::Caption } }) },
    };
    content = timerCard(std::move(content), { .duration = plan.focus }, phaseSteps(0), "Focus", std::to_string(t.pomodoroRounds) + " rounds",
                        { Button{ .glyph = Glyph::Play, .actionId = act::kStart, .compact = true, .primary = true } });
    return { .id = kPomodoroPanelId, .content = content, .wide = true };
}

// Compact: the countdown centred, the ring filling on the right; no phase
// name (the colour says it). Expanded (decision 107): the timer card, the
// phase and round over the time, its steps under it; the rest is in the
// window (decision 106). The widget (decision 019): the ring filling, the time left, the phase and round.
Content pomodoroContent(const PomodoroSession& s, const PomodoroPlan&)
{
    std::string name = phaseName(s.phase);
    std::string glance = s.timeline.playing ? name : "Paused";
    if (s.phase != Phase::LongBreak) glance += " · " + std::to_string(s.round) + "/" + std::to_string(tokens().pomodoroRounds);
    std::string detail = s.phase == Phase::LongBreak ? "last one" : roundsOf(static_cast<int>(s.round));
    if (!s.timeline.playing) detail = "paused · " + detail;
    TimeLabel tileLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Title };
    TimeLabel compactLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Body };
    Progress ring{ .timeline = s.timeline, .shape = ProgressShape::Ring };
    Button stop{ .glyph = Glyph::Stop, .actionId = act::kStop };
    Button pause{ .glyph = s.timeline.playing ? Glyph::Pause : Glyph::Play, .actionId = act::kPause, .compact = true, .primary = true }; // media's size: the card fits its height
    Button skip{ .glyph = Glyph::Next, .actionId = act::kSkip };
    Content content{
        .label = name,
        .accent = accentOf(s.phase),
        .compactLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .compactCenter = { row({ compactLeft }, Align::Center) },
        .compactTrailing = { row({ ring }, Align::Center) },
        .minimal = { row({ ring }, Align::Center) },
        // Same place on Home as the dormant tile, now live (decision 009).
        .tileLeading = { row({ ring }, Align::Center) },
        .tile = { row({ tileLeft }), row({ Text{ glance, TextStyle::Caption } }) },
    };
    return timerCard(std::move(content), s.timeline, phaseSteps(phasesDone(s)), name, detail, { stop, pause, skip });
}

} // namespace cup
