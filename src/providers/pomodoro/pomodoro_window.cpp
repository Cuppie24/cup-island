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

// "3h 20m": a statistic's value, short enough for a column (decision 064).
std::string shortSpent(Ms duration)
{
    long long all = duration.count() / 60000, hours = all / 60;
    return hours == 0 ? std::to_string(all) + "m" : std::to_string(hours) + "h " + std::to_string(all % 60) + "m";
}

std::string days(int n) { return std::to_string(n) + (n == 1 ? " day" : " days"); }

Row caption(const std::string& text) { return row({ Text{ .value = text, .style = TextStyle::Caption, .centred = true } }, Align::Center); }

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

// The header (decision 117): the time left, big, then stop, ▶/⏸ and skip;
// at the right, the phase and round over nothing else but the session's
// steps. Dormant: the planned focus, ▶, and stop and skip dimmed, so
// nothing moves when it starts. Stop asks with the host's question over the window (decision 126).
std::vector<Row> timerHeader(const PomodoroWindowView& v)
{
    const PomodoroSession* s = v.session ? &*v.session : nullptr;
    Timeline timeline = s ? s->timeline : Timeline{ .duration = v.plan.focus };
    TimeLabel time{ .timeline = timeline, .countdown = true, .style = TextStyle::Hero };
    // Only the phase: Focus, Break, Long break (decision 122). The steps beside it say where in the session.
    std::string where = s ? phaseName(s->phase) : phaseName(PomodoroPhase::Focus);
    bool playing = s && s->timeline.playing;
    Button stop{ .glyph = Glyph::Stop, .actionId = act::kStop, .enabled = s != nullptr, .secondary = true };
    Button main{ .glyph = playing ? Glyph::Pause : Glyph::Play, .actionId = s ? act::kPause : act::kStart, .primary = true };
    Button skip{ .glyph = Glyph::Next, .actionId = act::kSkip, .enabled = s != nullptr, .secondary = true };
    // The phase and round take the row's spare width, set against its end, so the loose row's
    // gap, not the edge's packed one, keeps them off the steps (decision 120).
    return { Row{ .items = { time, stop, main, skip, Text{ .value = where, .style = TextStyle::Caption, .alignEnd = true } },
                  .loose = true, // big, well-separated targets
                  .trailing = { phaseSteps(s ? phasesDone(*s) : 0, playing) } } };
}

// A group's card (decisions 117, 121): its name, edited in place, its delete
// at the end on hover; "Add task", the row being written under it if it's
// open (decision 132), then its tasks: open ones first, then solved ones,
// struck (decision 071). A task's box ticks it, a click on its text edits it,
// its delete shows on hover.
WindowSection groupCard(const TaskGroup& g, bool writing)
{
    namespace task = task_action;
    std::string gid = std::to_string(g.id);
    const auto& items = g.tasks.items();
    // Its name is plain text; its "…" holds Rename (a question with its name in a field) and Delete (decision 142),
    // and between them Delete solved tasks and Delete all tasks (decision 161).
    WindowSection card{ .title = g.name, .id = "group-" + gid };
    // Then its tasks' clears (the to-do list's, asked over the window), dim while there's nothing to clear.
    bool anyDone = std::any_of(items.begin(), items.end(), [](const Todo& t) { return t.done; });
    card.menu = { MenuItem{ .label = "Rename", .actionId = task::kAskGroupName + gid },
                  MenuItem{ .label = "Delete solved tasks", .actionId = inGroup(g.id, todo_action::kClearDone), .enabled = anyDone, .group = 1 },
                  MenuItem{ .label = "Delete all tasks", .actionId = inGroup(g.id, todo_action::kClearAll), .enabled = !items.empty(), .group = 1 },
                  MenuItem{ .label = "Delete", .actionId = task::kDeleteGroup + gid, .destructive = true, .group = 2 } };
    card.moveAction = task::kMoveGroup + gid; // dragged by its handle among the groups (decision 129)
    card.takesRows = true;                    // ...and a task from another group dropped in it
    // "+ Add task" is a button, the whole row its click, its + where a task's box is (decision 132);
    // it stays. Clicked, a task row being written opens under it with the keys: an unticked box,
    // its text where a task's is (decision 125). Enter adds one and keeps it for the next; left,
    // what's in it is added and the row goes; left blank or Esc'd, it just goes.
    // Lower than a task's row (decision 141): it's there on every group, so it takes less room.
    card.rows.push_back(Row{ .items = { Button{ .glyph = Glyph::Plus, .actionId = task::kWriteTask + gid, .compact = true, .low = true, .secondary = true },
                                        Text{ "Add task", TextStyle::Caption } },
                             .clickAnywhere = true,
                             .id = "add" });
    if (writing)
        card.rows.push_back(Row{ .items = { TextInput{ .actionId = inGroup(g.id, todo::kAdd), .placeholder = "New task", .focusedIcon = Glyph::Unchecked,
                                                       .maxLength = tokens().todoMaxLength, .leaveAction = task::kTaskWritten + gid, .autoFocus = true } },
                                 .id = "writing" });
    for (const Todo& t : items) {
        std::string id = std::to_string(t.id);
        Button box{ .glyph = t.done ? Glyph::Checked : Glyph::Unchecked, .actionId = inGroup(g.id, todo::kToggle + id), .compact = true };
        Text text{ .value = t.text, .style = t.done ? TextStyle::Caption : TextStyle::Body, .struck = t.done, .editAction = inGroup(g.id, todo::kEdit + id),
                   .editMaxLength = tokens().todoMaxLength, .wrap = true }; // long ones wrap (decision 045)
        Button remove{ .glyph = Glyph::Delete, .actionId = inGroup(g.id, todo::kDelete + id), .compact = true, .revealOnHover = true };
        // Dragged to another place in its group, or into another (decision 129).
        card.rows.push_back(Row{ .items = { box, text }, .trailing = { remove }, .moveAction = task::kMoveTask + gid + "." + id, .id = "task-" + id });
    }
    return card;
}

WindowPage timerPage(const PomodoroWindowView& v)
{
    WindowPage page{ .id = kPomodoroTimerPage, .title = "Timer", .glyph = Glyph::Timer, .titled = false, .header = timerHeader(v), .cards = true };
    // Under the header, the phase's progress, edge to edge (decision 122); dormant, an empty bar.
    page.headerProgress = v.session ? v.session->timeline : Timeline{ .duration = v.plan.focus };
    static const TaskBoard none;
    for (const TaskGroup& g : (v.tasks ? *v.tasks : none).groups()) page.sections.push_back(groupCard(g, v.writingIn == g.id));
    // Quiet (decision 122): a dashed outline, no fill; a caption's words; the whole row its button.
    page.sections.push_back({ .rows = { Row{ .items = { Button{ .glyph = Glyph::Plus, .actionId = task_action::kNewGroup, .compact = true, .secondary = true },
                                                      Text{ "New group", TextStyle::Caption } },
                                           .clickAnywhere = true } },
                              .id = "new",
                              .dashed = true });
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
    // Reset asks with the host's question over the window (decision 128).
    page.sections.push_back({ .rows = { groupItem(itemText("Reset statistics", "Every day before today is cleared."),
                                                  { Button{ .glyph = Glyph::Reset, .actionId = act::kResetStats, .compact = true } }) } });
    return page;
}

// The timer's settings (decisions 061, 064): the plan, then a circle per weekday for days off.
WindowPage settingsPage(const PomodoroWindowView& v)
{
    static const char* const kDayNames[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
    bool running = v.session.has_value();
    WindowPage page{ .id = kPomodoroSettingsPage, .title = "Settings", .subtitle = "The plan, and the days that don't count.", .glyph = Glyph::Settings,
                     .atFoot = true };
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

WindowContent pomodoroWindow(const PomodoroWindowView& v)
{
    std::optional<Color> accent = accentOf(v.session ? v.session->phase : Phase::Focus);
    return { .title = "Pomodoro", .accent = accent, .pages = { timerPage(v), statsPage(v), settingsPage(v) }, .rail = true };
}

} // namespace cup
