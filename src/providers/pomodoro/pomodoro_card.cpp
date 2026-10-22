#include "providers/pomodoro/pomodoro_card.h"

#include "core/tokens.h"

#include <string>

namespace cup {
namespace {

using Phase = PomodoroPhase;
namespace act = pomodoro_action;

std::string minutes(Ms duration) { return std::to_string(duration.count() / 60000) + " min"; }

// Time spent: "40 min", "1 h 5 min".
std::string spent(Ms duration)
{
    long long all = duration.count() / 60000, hours = all / 60;
    return hours == 0 ? minutes(duration) : std::to_string(hours) + " h " + std::to_string(all % 60) + " min";
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

// A length on the dormant card (decision 044): its name, the minutes, and
// a full-size − and + at the end (easy to hit), dimmed at either limit.
Row planRow(const char* name, Ms length, bool focus, const PomodoroPlan& plan)
{
    Button less{ .glyph = Glyph::Minus, .actionId = focus ? act::kFocusLess : act::kBreakLess, .enabled = canAdjust(plan, focus, -1) };
    Button more{ .glyph = Glyph::Plus, .actionId = focus ? act::kFocusMore : act::kBreakMore, .enabled = canAdjust(plan, focus, 1) };
    return Row{ .items = { Text{ name }, Text{ .value = minutes(length), .alignEnd = true } }, .trailing = { less, more }, .inset = true };
}

// The card's top row: its big time, and the gear in the top-right corner (decision 061).
Row topRow(Item big)
{
    return Row{ .items = { std::move(big) }, .align = Align::Center, .trailing = { Button{ Glyph::Settings, act::kOpenSettings } } };
}

Row caption(const std::string& text) { return row({ Text{ .value = text, .style = TextStyle::Caption, .centred = true } }, Align::Center); }

// A statistic on the dormant card (decision 061): what it is now, then at
// the end its best, as plain as the value (no caption), or "Record" when
// now beats it. Lined up with the plan rows (inset).
Row statRow(const std::string& now, int value, int best, std::vector<Item> before = {})
{
    Row r{ .items = { Text{ now } }, .leading = std::move(before), .inset = true };
    if (value > best && value > 0) r.items.push_back(Tag{ "Record" });
    else if (best > 0) r.items.push_back(Text{ .value = "best " + std::to_string(best), .alignEnd = true });
    return r;
}

// How it's going: sessions finished today, days in a row (a fire from three
// on), focus today. Shown whenever no session runs.
void appendGlance(Slot& card, const PomodoroGlance& g)
{
    card.push_back(statRow(std::to_string(g.finishedToday) + " finished today", g.finishedToday, g.bestDay));
    std::vector<Item> fire;
    if (g.daysInARow >= 3) fire.push_back(Icon{ Glyph::Fire });
    card.push_back(statRow(std::to_string(g.daysInARow) + (g.daysInARow == 1 ? " day" : " days") + " in a row", g.daysInARow, g.bestRun,
                           std::move(fire)));
    card.push_back(Row{ .items = { Text{ "Focused today" }, Text{ .value = spent(g.focusToday), .alignEnd = true } }, .inset = true });
}

const char* const kDayNames[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };

} // namespace

Color accentOf(Phase phase) { return phase == Phase::Focus ? tokens().pomodoroFocusColor : tokens().pomodoroBreakColor; }

PomodoroGlance glanceOf(const PomodoroStats& stats, const DaysOff& off, long long today)
{
    PomodoroDay day = countsOf(stats, today);
    Streaks runs = streaksOf(stats, off, today);
    return { .finishedToday = day.finished, .bestDay = bestDay(stats, today), .daysInARow = runs.current, .bestRun = runs.bestBefore,
             .focusToday = day.focus };
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
// last, each with − and + (decision 044), ▶ where the controls go, how it's
// going (decision 061), then the to-dos (decision 016). The widget is laid
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
        .expandedCenter = { topRow(Text{ .value = formatDuration(plan.focus), .style = TextStyle::Display, .centred = true }),
                            planRow("Focus", plan.focus, true, plan),
                            planRow("Break", plan.rest, false, plan),
                            row({ Button{ Glyph::Play, act::kStart } }, Align::Center) },
        .tileLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .tile = { row({ Text{ formatDuration(plan.focus), TextStyle::Title } }), row({ Text{ rounds, TextStyle::Caption } }) },
    };
    appendGlance(content.expandedCenter, glance);
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
    Slot page{ Row{ .items = { Text{ .value = "Timer settings", .style = TextStyle::Title, .centred = true } },
                    .leading = { Button{ Glyph::ChevronLeft, act::kBack } } },
               Row{ .items = { Text{ .value = "Days off", .style = TextStyle::Heading } }, .inset = true } };
    Row days{ .align = Align::Center }; // one row of circles, Monday first; a lit one is off
    for (std::size_t i = 0; i < off.size(); ++i)
        days.items.push_back(Button{ .actionId = act::kToggleDayOff + std::to_string(i), .on = off[i], .label = kDayNames[i], .round = true });
    page.push_back(std::move(days));
    page.push_back(caption("A day off doesn't break your days in a row."));
    if (fake) { // Debug builds: try the statistics with a made-up history, never saved
        const char* const shown[] = { "Real", "3 in a row, best 5", "New records" };
        page.push_back(Row{ .items = { Text{ .value = "Debug", .style = TextStyle::Heading } }, .section = true, .inset = true });
        page.push_back(Row{ .items = { Text{ "Fake statistics" }, Text{ .value = shown[*fake % 3], .style = TextStyle::Caption, .alignEnd = true },
                                       Button{ .glyph = Glyph::ChevronRight, .actionId = act::kFakeStats, .compact = true } },
                            .clickAnywhere = true,
                            .listItem = true });
    }
    card.expandedCenter = std::move(page);
    card.backAction = act::kBack; // Esc goes back to the timer, like the arrow (decision 033)
    return card;
}

} // namespace cup
