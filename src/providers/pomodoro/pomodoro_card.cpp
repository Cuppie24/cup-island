#include "providers/pomodoro/pomodoro_card.h"

#include "core/tokens.h"

#include <algorithm>
#include <string>

namespace cup {
namespace {

using Phase = PomodoroPhase;
namespace act = pomodoro_action;

// The card is laid out like media's (decisions 094, 106), so the two are
// one size: the ring where the cover goes, the time left big beside it and
// what it is under that, the expand button in the top-right corner (where
// media's app switch goes), then a bar with the time gone and left, and the
// controls, ▶/⏸ the filled circle between two bright ones.
Content timerCard(Content c, const Timeline& timeline, const std::string& what, std::vector<Item> controls)
{
    c.card = CardLayout::Standard;
    c.keepOpen = true;      // a click between the controls doesn't close it (Content::keepOpen)
    c.noIdleTimeout = true; // nor does being left alone: a timer kept in view (decision 062)
    c.expandedLeading = { row({ Progress{ .timeline = timeline, .shape = ProgressShape::Ring } }) };
    c.expandedCenter = { row({ TimeLabel{ .timeline = timeline, .countdown = true, .style = TextStyle::Display } }),
                         row({ Text{ what, TextStyle::Caption } }) };
    c.expandedTrailing = { row({ Button{ .glyph = Glyph::Expand, .actionId = act::kOpenWindow, .secondary = true } }) };
    Row buttons{ .items = std::move(controls), .align = Align::Center, .loose = true }; // big, well-separated targets
    c.expandedBottom = { row({ TimeLabel{ timeline }, Progress{ timeline }, TimeLabel{ timeline, true } }), buttons };
    return c;
}

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

// Dormant (decision 106): the planned focus in its empty ring, ▶ under it.
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
    content = timerCard(std::move(content), { .duration = plan.focus }, rounds,
                        { Button{ .glyph = Glyph::Play, .actionId = act::kStart, .compact = true, .primary = true } });
    return { .id = kPomodoroPanelId, .content = content, .wide = true };
}

// Compact: the countdown centred, the ring filling on the right; no phase
// name (the colour says it). Expanded (decision 106): the timer card, its
// phase and round under the time; the phase dots and the rest are in the
// window. The widget (decision 019): the ring filling, the time left, the phase and round.
Content pomodoroContent(const PomodoroSession& s, const PomodoroPlan&)
{
    std::string name = phaseName(s.phase);
    std::string glance = s.timeline.playing ? name : "Paused";
    if (s.phase != Phase::LongBreak) glance += " · " + std::to_string(s.round) + "/" + std::to_string(tokens().pomodoroRounds);
    std::string what = s.timeline.playing ? name : "Paused · " + name;
    if (s.phase != Phase::LongBreak) what += " · round " + std::to_string(s.round) + " of " + std::to_string(tokens().pomodoroRounds);
    TimeLabel tileLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Title };
    TimeLabel compactLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Body };
    Progress ring{ .timeline = s.timeline, .shape = ProgressShape::Ring };
    Button stop{ .glyph = Glyph::Stop, .actionId = act::kStop };
    Button pause{ .glyph = s.timeline.playing ? Glyph::Pause : Glyph::Play, .actionId = act::kPause, .compact = true, .primary = true };
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
    return timerCard(std::move(content), s.timeline, what, { stop, pause, skip });
}

} // namespace cup
