#include "providers/pomodoro/pomodoro_window.h"

#include "core/groups.h"
#include "core/tokens.h"

#include <algorithm>
#include <string>

namespace cup {
namespace {

using Phase = PomodoroPhase;
namespace act = pomodoro_action;
namespace todo = todo_action;

std::string minutes(Ms duration) { return std::to_string(duration.count() / 60000) + " min"; }

// Time spent: "40 min", "1 h 05 min".
std::string spent(Ms duration)
{
    long long all = duration.count() / 60000, hours = all / 60, mins = all % 60;
    if (hours == 0) return minutes(duration);
    return std::to_string(hours) + " h " + (mins < 10 ? "0" : "") + std::to_string(mins) + " min";
}

// "3h 20m": a statistic's value, short enough for a column (decision 064).
std::string shortSpent(Ms duration)
{
    long long all = duration.count() / 60000, hours = all / 60;
    return hours == 0 ? std::to_string(all) + "m" : std::to_string(hours) + "h " + std::to_string(all % 60) + "m";
}

std::string days(int n) { return std::to_string(n) + (n == 1 ? " day" : " days"); }

Row caption(const std::string& text) { return row({ Text{ .value = text, .style = TextStyle::Caption, .centred = true } }, Align::Center); }

// A question asked in place (a window has no dialogs over it): what it
// asks, what it means, then Cancel and the confirm button.
std::vector<Row> questionRows(const std::string& title, const std::string& message, const std::string& confirmLabel, const char* confirmAction)
{
    return { row({ Text{ .value = title, .style = TextStyle::Title, .centred = true } }, Align::Center),
             Row{ .items = { Text{ .value = message, .style = TextStyle::Caption, .centred = true, .wrap = true } }, .align = Align::Center },
             Row{ .items = { Button{ .actionId = act::kCancel, .label = "Cancel", .compact = true },
                             Button{ .actionId = confirmAction, .label = confirmLabel, .compact = true, .destructive = true } },
                  .align = Align::Center } };
}

// A length (decisions 044, 064): its name, then − the minutes +, grey until
// pointed at. The minutes keep a column as wide as the longest length, so
// both rows line up. While a session runs they're dimmed: the plan can't change under it.
Row planRow(const char* name, Ms length, bool focus, const PomodoroPlan& plan, bool running)
{
    const Tokens& t = tokens();
    std::string widest = minutes(std::max(t.pomodoroFocusMax, t.pomodoroBreakMax));
    Button less{ .glyph = Glyph::Minus, .actionId = focus ? act::kFocusLess : act::kBreakLess, .enabled = !running && canAdjust(plan, focus, -1),
                 .secondary = true };
    Button more{ .glyph = Glyph::Plus, .actionId = focus ? act::kFocusMore : act::kBreakMore, .enabled = !running && canAdjust(plan, focus, 1),
                 .secondary = true };
    return Row{ .items = { itemText(name) }, .trailing = { less, Text{ .value = minutes(length), .centred = true, .sizedAs = widest }, more } };
}

// The statistics are tables (decision 064): rows of three equal columns, a value over its caption.
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

// The timer: the dial, the phase dots and how many are done, the controls,
// or the stop question in their place (decisions 064, 065).
WindowSection timerSection(const PomodoroWindowView& v)
{
    WindowSection s;
    if (!v.session) {
        s.rows = { dialRow({ .duration = v.plan.focus }, "Focus", v.plan),
                   caption("Focus · " + std::to_string(tokens().pomodoroRounds) + " rounds of " + minutes(v.plan.focus)),
                   Row{ .items = { Button{ .glyph = Glyph::Play, .actionId = act::kStart, .primary = true } }, .align = Align::Center } };
        return s;
    }
    const PomodoroSession& session = *v.session;
    s.rows = { dialRow(session.timeline, phaseName(session.phase), v.plan),
               Row{ .items = { Steps{ phaseCount(), phasesDone(session) } }, .align = Align::Center },
               caption(progressCaption(session)) };
    if (v.question == PomodoroQuestion::Stop) {
        Dialog q = stopQuestion(session);
        for (Row& r : questionRows(q.title, q.message, q.confirmLabel, act::kStopConfirmed)) s.rows.push_back(std::move(r));
        return s;
    }
    Button pause{ .glyph = session.timeline.playing ? Glyph::Pause : Glyph::Play, .actionId = act::kPause, .primary = true };
    Button stop{ .glyph = Glyph::Stop, .actionId = act::kStop, .secondary = true };
    Button skip{ .glyph = Glyph::Next, .actionId = act::kSkip, .secondary = true };
    s.rows.push_back(Row{ .items = { stop, pause, skip }, .align = Align::Center, .loose = true });
    return s;
}

// Today: cycles finished and focus time, the days in a row (a fire from three on).
WindowSection todaySection(const PomodoroGlance& g)
{
    std::string cycles = std::to_string(g.finishedToday) + (g.finishedToday == 1 ? " cycle" : " cycles");
    WindowSection s{ .title = "Today", .rows = { groupItem(itemText(cycles, spent(g.focusToday) + " of focus")) } };
    if (g.daysInARow > 0)
        s.rows.push_back(Row{ .items = { itemText(days(g.daysInARow) + " in a row") },
                              .trailing = g.daysInARow >= 3 ? std::vector<Item>{ Icon{ Glyph::Fire } } : std::vector<Item>{} });
    return s;
}

// The to-dos (decision 016): the field, the list, the Done row, the clear
// buttons, or the clear question in their place; a deleted one's Undo first.
WindowSection todoSection(const PomodoroWindowView& v)
{
    WindowSection s{ .title = "To-dos" };
    if (v.undoDelete)
        s.rows.push_back(groupItem(itemText("To-do deleted"), { Button{ .actionId = todo::kUndoDelete, .label = "Undo", .compact = true } }));
    TodoList none;
    Slot rows;
    appendTodoRows(rows, v.todos ? *v.todos : none);
    if (v.question == PomodoroQuestion::ClearDone || v.question == PomodoroQuestion::ClearAll) {
        const char* id = v.question == PomodoroQuestion::ClearDone ? todo::kClearDone : todo::kClearAll;
        Dialog q = clearQuestion(id, v.todos ? *v.todos : none, kPomodoroPanelId);
        if (!rows.empty() && !rows.back().items.empty() && std::holds_alternative<Button>(rows.back().items.front())) rows.pop_back(); // the clear buttons
        for (Row& r : questionRows(q.title, q.message, q.confirmLabel, confirmActionOf(v.question))) rows.push_back(std::move(r));
    }
    for (Row& r : rows) s.rows.push_back(std::move(r));
    return s;
}

WindowPage timerPage(const PomodoroWindowView& v)
{
    WindowPage page{ .id = kPomodoroTimerPage, .title = "Timer", .glyph = Glyph::Timer };
    page.sections = { timerSection(v), todaySection(v.glance), todoSection(v) };
    return page;
}

// Today, then the personal records (decisions 061, 064), each a table: what
// beats its best is tagged "Record", and the record shown is then the new one.
WindowPage statsPage(const PomodoroWindowView& v)
{
    const PomodoroGlance& g = v.glance;
    bool cyclesRecord = g.finishedToday > g.bestDay && g.finishedToday > 0, focusRecord = g.focusToday > g.bestFocus && g.focusToday >= Ms{ 60000 },
         runRecord = g.daysInARow > g.bestRun && g.daysInARow > 0;
    std::string run = (g.daysInARow >= 3 ? "🔥 " : "") + std::to_string(g.daysInARow); // the fire from three on
    WindowPage page{ .id = kPomodoroStatsPage, .title = "Statistics", .subtitle = "How your focus is going.", .glyph = Glyph::Chart };
    page.sections.push_back({ .title = "Today",
                              .rows = { valuesRow({ std::to_string(g.finishedToday), shortSpent(g.focusToday), run }),
                                        captionsRow({ { "cycles", cyclesRecord }, { "focus time", focusRecord }, { "days in a row", runRecord } }) } });
    page.sections.push_back({ .title = "🏆 Personal records",
                              .rows = { valuesRow({ std::to_string(std::max(g.finishedToday, g.bestDay)), shortSpent(std::max(g.focusToday, g.bestFocus)),
                                                    days(std::max(g.daysInARow, g.bestRun)) }),
                                        captionsRow({ { "most cycles", false }, { "best focus day", false }, { "best streak", false } }) } });
    WindowSection reset;
    if (v.question == PomodoroQuestion::ResetStats) {
        Dialog q = resetQuestion();
        reset.rows = questionRows(q.title, q.message, q.confirmLabel, act::kResetStatsConfirmed);
    } else {
        reset.rows = { groupItem(itemText("Reset statistics", "Every day before today is cleared."),
                                 { Button{ .glyph = Glyph::Reset, .actionId = act::kResetStats, .compact = true } }) };
    }
    page.sections.push_back(std::move(reset));
    return page;
}

// The timer's settings (decisions 061, 064): the plan, then a circle per weekday for days off.
WindowPage settingsPage(const PomodoroWindowView& v)
{
    static const char* const kDayNames[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
    bool running = v.session.has_value();
    WindowPage page{ .id = kPomodoroSettingsPage, .title = "Settings", .subtitle = "The plan, and the days that don't count.", .glyph = Glyph::Settings };
    page.sections.push_back({ .title = running ? "Plan · while the timer runs, it stays as it is" : "Plan",
                              .rows = { planRow("Focus", v.plan.focus, true, v.plan, running), planRow("Break", v.plan.rest, false, v.plan, running) } });
    Row circles{ .align = Align::Center }; // Monday first; a lit one is off
    for (std::size_t i = 0; i < v.daysOff.size(); ++i)
        circles.items.push_back(Button{ .actionId = act::kToggleDayOff + std::to_string(i), .on = v.daysOff[i], .label = kDayNames[i], .round = true });
    page.sections.push_back({ .title = "Days off", .rows = { circles, caption("A day off doesn't break your days in a row.") } });
    if (v.fake) { // Debug builds: try the statistics with a made-up history, never saved
        const char* const shown[] = { "Real", "3 in a row, best 5", "New records" };
        page.sections.push_back({ .title = "Debug",
                                  .rows = { groupItem(itemText("Fake statistics", shown[*v.fake % 3]),
                                                      { Button{ .glyph = Glyph::ChevronRight, .actionId = act::kFakeStats, .compact = true } }) } });
    }
    return page;
}

} // namespace

const char* confirmActionOf(PomodoroQuestion question)
{
    switch (question) {
    case PomodoroQuestion::Stop: return act::kStopConfirmed;
    case PomodoroQuestion::ResetStats: return act::kResetStatsConfirmed;
    case PomodoroQuestion::ClearDone: return todo::kClearDoneConfirmed;
    case PomodoroQuestion::ClearAll: return todo::kClearAllConfirmed;
    case PomodoroQuestion::None: break;
    }
    return "";
}

WindowContent pomodoroWindow(const PomodoroWindowView& v)
{
    std::optional<Color> accent = accentOf(v.session ? v.session->phase : Phase::Focus);
    return { .title = "Focus timer", .accent = accent, .pages = { timerPage(v), statsPage(v), settingsPage(v) } };
}

} // namespace cup
