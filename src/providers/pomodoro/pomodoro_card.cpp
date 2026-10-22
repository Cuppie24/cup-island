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

// A length, the settings page's first block (decisions 044, 064): its name,
// then − the minutes + at the end, grey until pointed at. The minutes keep a
// column as wide as the longest length, so both rows line up. While a session
// runs they're dimmed: the plan can't change under it.
Row planRow(const char* name, Ms length, bool focus, const PomodoroPlan& plan, bool running)
{
    const Tokens& t = tokens();
    std::string widest = minutes(std::max(t.pomodoroFocusMax, t.pomodoroBreakMax));
    Button less{ .glyph = Glyph::Minus, .actionId = focus ? act::kFocusLess : act::kBreakLess, .enabled = !running && canAdjust(plan, focus, -1),
                 .secondary = true };
    Button more{ .glyph = Glyph::Plus, .actionId = focus ? act::kFocusMore : act::kBreakMore, .enabled = !running && canAdjust(plan, focus, 1),
                 .secondary = true };
    return Row{ .items = { Text{ name } }, .trailing = { less, Text{ .value = minutes(length), .centred = true, .sizedAs = widest }, more },
                .inset = true };
}

// "3h 20m": a statistic's value, short enough for a column (decision 064).
std::string shortSpent(Ms duration)
{
    long long all = duration.count() / 60000, hours = all / 60;
    return hours == 0 ? std::to_string(all) + "m" : std::to_string(hours) + "h " + std::to_string(all % 60) + "m";
}

std::string days(int n) { return std::to_string(n) + (n == 1 ? " day" : " days"); }

// The card's top row: the time in a ring that fills as it goes, the phase
// under it (decision 065), the ring one size whatever the phase (sized for
// the plan's longest). In the top-right corner the statistics and the gear,
// grey until pointed at (decision 066); while dormant, the days in a row in
// the top-left one (a fire from three on). The corners sit at the ring's top
// (Row::edgesAtTop), and the ring stays centred on the card.
Row dialRow(const Timeline& timeline, const std::string& label, const PomodoroPlan& plan, int daysInARow = -1) // -1: no corner
{
    Ms longest = std::max({ plan.focus, plan.rest, tokens().pomodoroLongBreak });
    Row r{ .items = { Dial{ .timeline = timeline, .label = label, .sizedFor = longest } }, .align = Align::Center,
           .trailing = { Button{ .glyph = Glyph::Chart, .actionId = act::kOpenStats, .secondary = true },
                         Button{ .glyph = Glyph::Settings, .actionId = act::kOpenSettings, .secondary = true } },
           .edgesAtTop = true };
    if (daysInARow >= 3) r.leading.push_back(Icon{ Glyph::Fire });
    if (daysInARow >= 0) r.leading.push_back(Text{ .value = days(daysInARow), .style = TextStyle::Caption });
    return r;
}

Row caption(const std::string& text) { return row({ Text{ .value = text, .style = TextStyle::Caption, .centred = true } }, Align::Center); }

// Today under ▶, a group of its own (decisions 063, 066): cycles (sessions)
// finished and focus time, quiet. The chart button opens the rest.
Row todayRow(const PomodoroGlance& g)
{
    std::string text = std::to_string(g.finishedToday) + (g.finishedToday == 1 ? " cycle" : " cycles") + " · " + spent(g.focusToday);
    return Row{ .items = { Text{ .value = text, .style = TextStyle::Caption, .centred = true } }, .align = Align::Center, .hug = true,
                .apart = true };
}

// The statistics page is a table (decision 064): rows of three equal
// columns (Row::even), a value row over its caption row, so they line up.
Row valuesRow(std::initializer_list<std::string> values)
{
    Row r{ .align = Align::Center, .even = true };
    for (const std::string& v : values) r.items.push_back(Text{ .value = v, .style = TextStyle::Title, .centred = true });
    return r;
}

// What each value is; a "Record" tag in its place when it beats its best.
Row captionsRow(std::initializer_list<std::pair<const char*, bool>> captions)
{
    Row r{ .align = Align::Center, .even = true };
    for (auto [text, record] : captions)
        if (record) r.items.push_back(Tag{ "Record" });
        else r.items.push_back(Text{ .value = text, .style = TextStyle::Caption, .centred = true });
    return r;
}

Row pageTitle(const char* title)
{
    return Row{ .items = { Text{ .value = title, .style = TextStyle::Title, .centred = true } },
                .leading = { Button{ .glyph = Glyph::ChevronLeft, .actionId = act::kBack, .secondary = true } } };
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

// Dormant (decisions 061, 063, 064, 065): the focus time in its empty ring,
// "Focus" under it, then ▶, the card's one main action, today's line, and the
// to-dos (decision 016). The plan is on the settings page; days in a row in the top-left
// corner. The widget is laid out like the running one: the timer icon where
// the ring goes, the focus time, the rounds.
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
        .expandedCenter = { dialRow({ .duration = plan.focus }, "Focus", plan, glance.daysInARow),
                            Row{ .items = { Button{ .glyph = Glyph::Play, .actionId = act::kStart, .primary = true } }, .align = Align::Center,
                                 .apart = true } }, // a group of its own (decision 066)
        .tileLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .tile = { row({ Text{ formatDuration(plan.focus), TextStyle::Title } }), row({ Text{ rounds, TextStyle::Caption } }) },
    };
    content.expandedCenter.push_back(todayRow(glance));
    appendTodoRows(content.expandedCenter, todos);
    return { .id = kPomodoroPanelId, .content = content, .wide = true };
}

// Compact: the countdown centred, the ring filling on the right; no phase
// name (the colour says it). Expanded, a centred card: the dial (the time
// left in a ring filling with the phase, its name under it, decision 065), a
// dot per phase and how many are done, then the controls, right above the
// to-dos they're about. The widget (decision 019): the ring filling, the
// time left, the phase and round.
Content pomodoroContent(const PomodoroSession& s, const TodoList& todos, const PomodoroPlan& plan)
{
    std::string name = nameOf(s.phase);
    int done = phasesDone(s), total = phaseCount();
    std::string caption = std::to_string(done) + " of " + std::to_string(total) + " done"; // the dial says the phase
    if (!s.timeline.playing) caption = "Paused · " + caption;
    std::string glance = s.timeline.playing ? name : "Paused";
    if (s.phase != Phase::LongBreak) glance += " · " + std::to_string(s.round) + "/" + std::to_string(tokens().pomodoroRounds);
    TimeLabel tileLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Title };
    TimeLabel compactLeft{ .timeline = s.timeline, .countdown = true, .style = TextStyle::Body };
    Progress ring{ .timeline = s.timeline, .shape = ProgressShape::Ring };
    Button pause{ .glyph = s.timeline.playing ? Glyph::Pause : Glyph::Play, .actionId = act::kPause, .primary = true };
    Button stop{ .glyph = Glyph::Stop, .actionId = act::kStop, .secondary = true };
    Button skip{ .glyph = Glyph::Next, .actionId = act::kSkip, .secondary = true };
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
            dialRow(s.timeline, name, plan), // the ring fills as the phase goes: no bar (decision 065)
            Row{ .items = { Steps{ total, done } }, .align = Align::Center, .apart = true }, // groups apart (decision 066)
            row({ Text{ .value = caption, .style = TextStyle::Caption, .centred = true } }, Align::Center),
            Row{ .items = { stop, pause, skip }, .align = Align::Center, .loose = true, .apart = true }, // the main one in the middle (decision 064)
        },
        // Same place on Home as the dormant tile, now live (decision 009).
        .tileLeading = { row({ ring }, Align::Center) },
        .tile = { row({ tileLeft }), row({ Text{ glance, TextStyle::Caption } }) },
    };
    appendTodoRows(content.expandedCenter, todos); // what to work on, under the timer (decision 016)
    return content;
}

// The timer's settings (decisions 061, 064), like a page of Settings
// (decision 030): a title with a back arrow, the plan first, then a circle per
// weekday for days off.
Content withSettingsPage(Content card, const PomodoroPlan& plan, bool running, const DaysOff& off, std::optional<int> fake)
{
    Slot page{ pageTitle("Timer settings"), planRow("Focus", plan.focus, true, plan, running), planRow("Break", plan.rest, false, plan, running),
               heading("Days off", true) };
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

// Today, then the personal records under a line (decisions 061, 064), each
// a table of three columns: a value over what it is. What beats its best is
// tagged "Record", and the record shown is then the new one.
Content withStatsPage(Content card, const PomodoroGlance& g)
{
    bool cyclesRecord = g.finishedToday > g.bestDay && g.finishedToday > 0, focusRecord = g.focusToday > g.bestFocus && g.focusToday >= Ms{ 60000 },
         runRecord = g.daysInARow > g.bestRun && g.daysInARow > 0;
    std::string run = (g.daysInARow >= 3 ? "🔥 " : "") + std::to_string(g.daysInARow); // the fire from three on, as in the corner
    card.expandedCenter = { pageTitle("Statistics"),
                            caption("Today"),
                            valuesRow({ std::to_string(g.finishedToday), shortSpent(g.focusToday), run }),
                            captionsRow({ { "cycles", cyclesRecord }, { "focus time", focusRecord }, { "days in a row", runRecord } }),
                            Row{ .items = { Divider{} }, .section = true },
                            caption("🏆 Personal records"),
                            valuesRow({ std::to_string(std::max(g.finishedToday, g.bestDay)), shortSpent(std::max(g.focusToday, g.bestFocus)),
                                        days(std::max(g.daysInARow, g.bestRun)) }),
                            captionsRow({ { "most cycles", false }, { "best focus day", false }, { "best streak", false } }) };
    card.card = CardLayout::Narrow; // three short columns: kept together in the middle, not at the card's edges
    card.backAction = act::kBack; // Esc goes back to the timer, like the arrow (decision 033)
    return card;
}

} // namespace cup
