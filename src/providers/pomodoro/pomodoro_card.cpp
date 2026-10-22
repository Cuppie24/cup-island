#include "providers/pomodoro/pomodoro_card.h"

#include "core/tokens.h"

#include <algorithm>
#include <string>

namespace cup {
namespace {

using Phase = PomodoroPhase;
namespace act = pomodoro_action;

std::string minutes(Ms duration) { return std::to_string(duration.count() / 60000) + " min"; }

// Time spent: "40 min", "1 h 05 min".
std::string spent(Ms duration)
{
    long long all = duration.count() / 60000, hours = all / 60, mins = all % 60;
    if (hours == 0) return minutes(duration);
    return std::to_string(hours) + " h " + (mins < 10 ? "0" : "") + std::to_string(mins) + " min";
}

const char* nameOf(Phase phase)
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

// A length on the dormant card (decisions 044, 063): its name, quiet, then
// − the minutes + at the end. − and + show while the pointer is on the row
// (or the keyboard is), but always take their room, and the minutes keep a
// column as wide as the longest length: both rows line up, buttons or not.
Row planRow(const char* name, Ms length, bool focus, const PomodoroPlan& plan)
{
    const Tokens& t = tokens();
    std::string widest = minutes(std::max(t.pomodoroFocusMax, t.pomodoroBreakMax));
    Button less{ .glyph = Glyph::Minus, .actionId = focus ? act::kFocusLess : act::kBreakLess, .enabled = canAdjust(plan, focus, -1),
                 .revealOnHover = true };
    Button more{ .glyph = Glyph::Plus, .actionId = focus ? act::kFocusMore : act::kBreakMore, .enabled = canAdjust(plan, focus, 1),
                 .revealOnHover = true };
    return Row{ .items = { Text{ .value = name, .style = TextStyle::Caption } },
                .trailing = { less, Text{ .value = minutes(length), .centred = true, .sizedAs = widest }, more },
                .inset = true };
}

std::string days(int n) { return std::to_string(n) + (n == 1 ? " day" : " days"); }

// The card's top row: its big time, the gear in the top-right corner, and
// while dormant the days in a row in the top-left one (a fire from three
// on), quiet, a way into the statistics page (decisions 061, 063).
Row topRow(Item big, int daysInARow = -1) // -1: no corner (the running card)
{
    // Hugging: the big time as wide as it is, so it centres on the card, not in the room the corners leave.
    Row r{ .items = { std::move(big) }, .align = Align::Center, .trailing = { Button{ Glyph::Settings, act::kOpenSettings } }, .hug = true };
    if (daysInARow >= 3) r.leading.push_back(Icon{ Glyph::Fire });
    if (daysInARow >= 0) r.leading.push_back(Button{ .actionId = act::kOpenStats, .label = days(daysInARow), .quiet = true });
    return r;
}

Row caption(const std::string& text) { return row({ Text{ .value = text, .style = TextStyle::Caption, .centred = true } }, Align::Center); }

// Today under ▶, a group of its own (decision 063): cycles (sessions)
// finished and focus time, quiet, one click into the statistics page.
Row todayRow(const PomodoroGlance& g)
{
    std::string text = std::to_string(g.finishedToday) + (g.finishedToday == 1 ? " cycle" : " cycles") + " · " + spent(g.focusToday);
    return Row{ .items = { Button{ .actionId = act::kOpenStats, .label = text, .quiet = true } }, .align = Align::Center, .section = true };
}

// A row of the statistics page: its name, a "Record" tag beside it when
// `record`, and its value at the end. Inset like the plan rows.
Row statRow(const char* name, const std::string& value, bool record = false)
{
    Row r{ .items = { Text{ name } }, .inset = true };
    if (record) r.items.push_back(Tag{ "Record" });
    r.items.push_back(Text{ .value = value, .alignEnd = true });
    return r;
}

Row pageTitle(const char* title)
{
    return Row{ .items = { Text{ .value = title, .style = TextStyle::Title, .centred = true } }, .leading = { Button{ Glyph::ChevronLeft, act::kBack } } };
}

Row heading(const char* text, bool section)
{
    return Row{ .items = { Text{ .value = text, .style = TextStyle::Heading } }, .section = section, .inset = true };
}

const char* const kDayNames[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };

} // namespace

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

// Dormant: a centred card like the running one, so starting changes as
// little as possible: the focus time big, then how long focus and breaks
// last, each with − and + (decision 044), ▶ where the controls go, today's
// statistics (decision 061), then the to-dos (decision 016). The widget is laid
// out like the running one too: the timer icon where the ring goes, the focus time, the rounds.
Panel pomodoroPanel(const TodoList& todos, const PomodoroPlan& plan, const PomodoroGlance& glance)
{
    const Tokens& t = tokens();
    std::string rounds = "Focus · " + std::to_string(t.pomodoroRounds) + " rounds";
    Content content{
        .label = "Focus",
        .accent = accentOf(Phase::Focus),
        .card = CardLayout::Centred,
        .keepOpen = true, // a click between the controls or to-dos doesn't close it (Content::keepOpen)
        .noIdleTimeout = true, // nor does being left alone: a timer kept in view (decision 062)
        .expandedCenter = { topRow(Text{ .value = formatDuration(plan.focus), .style = TextStyle::Display, .centred = true }, glance.daysInARow),
                            planRow("Focus", plan.focus, true, plan),
                            planRow("Break", plan.rest, false, plan),
                            Row{ .items = { Button{ Glyph::Play, act::kStart } }, .align = Align::Center, .section = true } }, // apart from the plan
        .tileLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .tile = { row({ Text{ formatDuration(plan.focus), TextStyle::Title } }), row({ Text{ rounds, TextStyle::Caption } }) },
    };
    content.expandedCenter.push_back(todayRow(glance));
    appendTodoRows(content.expandedCenter, todos);
    return { .id = kPomodoroPanelId, .content = content, .wide = true };
}

// Compact: the countdown centred, the ring filling on the right; no phase
// name (the colour says it). Expanded, a centred card (decision 014): a big
// countdown on top, a bar for the phase under it (as wide as the dots), a
// dot per phase and what they mean, then the controls, right above the
// to-dos they're about. The ring is for compact,
// the bubble and the widget, where there's no room for a big countdown.
// The widget (decision 019): the ring filling, the time left, the phase and round.
Content pomodoroContent(const PomodoroSession& s, const TodoList& todos)
{
    std::string name = nameOf(s.phase);
    int done = phasesDone(s), total = phaseCount();
    std::string caption = name + " · " + std::to_string(done) + " of " + std::to_string(total) + " done";
    if (!s.timeline.playing) caption = "Paused · " + caption;
    std::string glance = s.timeline.playing ? name : "Paused";
    if (s.phase != Phase::LongBreak) glance += " · " + std::to_string(s.round) + "/" + std::to_string(tokens().pomodoroRounds);
    TimeLabel tileLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Title };
    TimeLabel compactLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Body };
    TimeLabel bigLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Display };
    Progress ring{ .timeline = s.timeline, .shape = ProgressShape::Ring };
    Button pause{ s.timeline.playing ? Glyph::Pause : Glyph::Play, act::kPause };
    Button stop{ Glyph::Stop, act::kStop };
    Content content{
        .label = name,
        .accent = accentOf(s.phase),
        .card = CardLayout::Centred,
        .keepOpen = true, // a click between the controls or to-dos doesn't close it (Content::keepOpen)
        .noIdleTimeout = true, // nor does being left alone: a timer kept in view (decision 062)
        .compactLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .compactCenter = { row({ compactLeft }, Align::Center) },
        .compactTrailing = { row({ ring }, Align::Center) },
        .minimal = { row({ ring }, Align::Center) },
        .expandedCenter = {
            topRow(bigLeft),
            Row{ .items = { Progress{ .timeline = s.timeline } }, .span = RowSpan::MatchBelow }, // as wide as the dots
            row({ Steps{ total, done } }, Align::Center),
            row({ Text{ .value = caption, .style = TextStyle::Caption, .centred = true } }, Align::Center),
            Row{ .items = { pause, Button{ Glyph::Next, act::kSkip }, stop }, .align = Align::Center, .loose = true },
        },
        // Same place on Home as the dormant tile, now live (decision 009).
        .tileLeading = { row({ ring }, Align::Center) },
        .tile = { row({ tileLeft }), row({ Text{ glance, TextStyle::Caption } }) },
    };
    appendTodoRows(content.expandedCenter, todos); // what to work on, under the timer (decision 016)
    return content;
}

// The timer's settings (decision 061), like a page of Settings (decision
// 030): a title with a back arrow, then a circle per weekday for days off.
Content withSettingsPage(Content card, const DaysOff& off, std::optional<int> fake)
{
    Slot page{ pageTitle("Timer settings"), heading("Days off", false) };
    Row days{ .align = Align::Center }; // one row of circles, Monday first; a lit one is off
    for (std::size_t i = 0; i < off.size(); ++i)
        days.items.push_back(Button{ .actionId = act::kToggleDayOff + std::to_string(i), .on = off[i], .label = kDayNames[i], .round = true });
    page.push_back(std::move(days));
    page.push_back(caption("A day off doesn't break your days in a row."));
    if (fake) { // Debug builds: try the statistics with a made-up history, never saved
        const char* const shown[] = { "Real", "3 in a row, best 5", "New records" };
        page.push_back(heading("Debug", true));
        page.push_back(Row{ .items = { Text{ "Fake statistics" }, Text{ .value = shown[*fake % 3], .style = TextStyle::Caption, .alignEnd = true },
                                       Button{ .glyph = Glyph::ChevronRight, .actionId = act::kFakeStats, .compact = true } },
                            .clickAnywhere = true,
                            .listItem = true });
    }
    card.expandedCenter = std::move(page);
    card.backAction = act::kBack; // Esc goes back to the timer, like the arrow (decision 033)
    return card;
}

// Now, then the bests (decision 061): what beats its best is tagged
// "Record", and the best shown is then the new one.
Content withStatsPage(Content card, const PomodoroGlance& g)
{
    bool dayRecord = g.finishedToday > g.bestDay, runRecord = g.daysInARow > g.bestRun, focusRecord = g.focusToday > g.bestFocus;
    card.expandedCenter = { pageTitle("Statistics"),
                            heading("Now", false),
                            statRow("Finished today", std::to_string(g.finishedToday), dayRecord && g.finishedToday > 0),
                            statRow("Days in a row", days(g.daysInARow), runRecord && g.daysInARow > 0),
                            statRow("Focused today", spent(g.focusToday), focusRecord && g.focusToday >= Ms{ 60000 }),
                            heading("Best", true),
                            statRow("Finished in a day", std::to_string(std::max(g.finishedToday, g.bestDay))),
                            statRow("Days in a row", days(std::max(g.daysInARow, g.bestRun))),
                            statRow("Focused in a day", spent(std::max(g.focusToday, g.bestFocus))) };
    card.card = CardLayout::Narrow; // short names and values: kept together in the middle, not at the card's edges
    card.backAction = act::kBack; // Esc goes back to the timer, like the arrow (decision 033)
    return card;
}

} // namespace cup
