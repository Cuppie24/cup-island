// Pomodoro's statistics and its settings page (decision 061): days, days in
// a row with days off, what's kept, and the provider counting a real session.

#include "core/layout.h"
#include "core/tokens.h"
#include "core/window_layout.h"
#include "expect.h"
#include "providers/pomodoro/pomodoro_provider.h"
#include "providers/provider_registry.h"

#include <map>

namespace cup::test {
namespace {

constexpr long long kDay = 86'400'000;
const long long kMonday = 20'381; // 2025-10-20, a Monday

PomodoroStats finishedOn(std::initializer_list<long long> days)
{
    PomodoroStats stats;
    for (long long d : days) ++dayOf(stats, d).finished;
    return stats;
}

void pureTests()
{
    expect(weekday(0) == 3 && weekday(kMonday) == 0 && weekday(kMonday + 6) == 6 && weekday(-1) == 2, "weekdays: Monday is 0");
    expect(localDay(Ms{ kDay - 1 }, Ms{ 0 }) == 0 && localDay(Ms{ kDay - 1 }, Ms{ 3'600'000 }) == 1 && localDay(Ms{ 0 }, Ms{ -1 }) == -1,
           "the local day: the offset moves midnight");
    expect(untilMidnight(Ms{ kDay - 1000 }, Ms{ 0 }) == Ms{ 1000 } && untilMidnight(Ms{ 0 }, Ms{ 0 }) == Ms{ kDay }, "until midnight");

    PomodoroStats stats;
    dayOf(stats, 10).finished = 1;
    dayOf(stats, 5).finished = 3;
    dayOf(stats, 10).todosDone = 2;
    expect(stats.days.size() == 2 && stats.days[0].day == 5 && stats.days[1].todosDone == 2, "days kept in order, one each");
    expect(countsOf(stats, 7) == PomodoroDay{ .day = 7 } && countsOf(stats, 5).finished == 3, "a day with nothing: zeros");
    expect(bestDay(stats) == 3 && bestDay(stats, 5) == 1 && totals(stats).finished == 4 && totals(stats).todosDone == 2,
           "the best day (leaving one out), and totals");

    // Saved and loaded: every count, the dates readable.
    stats.days = { { .day = kMonday, .focus = Ms{ 6'000'000 }, .started = 3, .finished = 2, .canceled = 1, .focusDone = 7,
                     .focusSkipped = 1, .breaksSkipped = 2, .todosDone = 5 },
                   { .day = kMonday + 1, .finished = 1 } };
    std::string saved = saveStats(stats);
    expect(saved.find("2025-10-20 6000000 3 2 1 7 1 2 5\n") != std::string::npos, "a day per line, its date readable");
    expect(loadStats(saved) == stats, "saved and loaded");
    expect(loadStats("") == PomodoroStats{} && loadStats("junk") == PomodoroStats{}, "unreadable: no stats");
    PomodoroStats partly = loadStats("stats1\n2025-13-01 1 1 1 1 1 1 1 1\nnonsense\n2025-10-21 60000 1 1 0 4 0 0 0\n");
    expect(partly.days.size() == 1 && partly.days[0].day == kMonday + 1, "unreadable lines skipped");

    // A finished session: every phase, at most one focus phase skipped (tokens.json).
    expect(sessionCounts(0) && sessionCounts(1) && !sessionCounts(2), "one focus phase may be skipped, not two");

    // Days in a row.
    DaysOff none{};
    long long sun = kMonday + 6;
    expect(daysInARow({}, none, sun) == 0, "nothing yet: none");
    expect(daysInARow(finishedOn({ sun - 2, sun - 1, sun }), none, sun) == 3, "three days up to today");
    expect(daysInARow(finishedOn({ sun - 2, sun - 1 }), none, sun) == 2, "today not yet: doesn't break it");
    expect(daysInARow(finishedOn({ sun - 3, sun - 1 }), none, sun) == 1, "a day missed breaks it");
    PomodoroStats started;
    ++dayOf(started, sun - 1).started; // started, never finished
    dayOf(started, sun - 2).finished = 1;
    expect(daysInARow(started, none, sun) == 0, "a day with only a stopped session doesn't count");
    DaysOff weekend{};
    weekend[5] = weekend[6] = true;
    long long nextMon = kMonday + 7, fri = kMonday + 4;
    expect(daysInARow(finishedOn({ fri - 1, fri, nextMon }), weekend, nextMon) == 3, "a weekend off doesn't break it");
    expect(daysInARow(finishedOn({ fri, fri + 1, nextMon }), weekend, nextMon) == 3, "a day off worked counts");
    expect(daysInARow(finishedOn({ fri }), weekend, nextMon) == 1, "Monday not yet: still Friday's");
    expect(daysInARow(finishedOn({ fri - 1 }), weekend, nextMon) == 0, "Friday missed: broken");

    // The best run before the current one: what a record has to beat.
    expect(streaksOf(finishedOn({ 1, 2, 3, 4, 5, 8, 9 }), none, 9) == Streaks{ 2, 5 }, "a run of 2 now, 5 before");
    expect(streaksOf(finishedOn({ 1, 2, 5, 6, 7 }), none, 8) == Streaks{ 3, 2 }, "today not yet: the run goes on");
    expect(streaksOf(finishedOn({ 1, 2, 3 }), none, 9) == Streaks{ 0, 3 }, "broken: none now, the old run the best");
    expect(streaksOf({}, none, 9) == Streaks{}, "nothing: none");

    // Debug builds' made-up histories.
    expect(streaksOf(fakeStats(kMonday + 30, 1), none, kMonday + 30) == Streaks{ 3, 5 } &&
           countsOf(fakeStats(kMonday + 30, 1), kMonday + 30).finished == 2 && bestDay(fakeStats(kMonday + 30, 1), kMonday + 30) == 4,
           "fake 1: 3 in a row (best 5), 2 today (best 4)");
    expect(streaksOf(fakeStats(kMonday + 30, 2), none, kMonday + 30) == Streaks{ 6, 5 } &&
           countsOf(fakeStats(kMonday + 30, 2), kMonday + 30).finished == 5, "fake 2: both records");
    expect(fakeStats(kMonday, 0).days.empty(), "fake 0: none");

    DaysOff off{};
    off[2] = off[6] = true;
    expect(saveDaysOff(off) == "daysoff1 0010001\n" && loadDaysOff(saveDaysOff(off)) == off, "days off saved and loaded");
    expect(loadDaysOff("") == DaysOff{} && loadDaysOff("daysoff1 01") == DaysOff{}, "unreadable: none off");
}

// The provider through the registry, with its store and a wall clock on Monday morning.
struct Timer {
    Ms now{ 0 };
    Ms wall{ kMonday * kDay + 9 * 3'600'000 };
    std::map<std::string, std::string> store;
    std::vector<Activity> live;
    std::vector<Panel> panels;
    std::vector<Alert> alerts;
    std::vector<Dialog> asked;
    ProviderRegistry registry;

    explicit Timer(bool debug = false)
        : registry([this](const Event& e, const char*) { record(e); }, [this] { return now; },
                   { .load = [this](const std::string&, const std::string& key) { return store[key]; },
                     .save = [this](const std::string&, const std::string& key, const std::string& text) { store[key] = text; },
                     .wallClock = [this] { return wall; },
                     .utcOffset = [] { return Ms{ 0 }; } })
    {
        registry.add(std::make_unique<PomodoroProvider>(debug));
        registry.start("pomodoro");
        act(pomodoro_action::kOpenWindow); // its window, where today and the statistics are (decision 106)
    }

    void record(const Event& e)
    {
        if (auto* p = std::get_if<PanelPublished>(&e)) panels.push_back(p->panel);
        if (auto* a = std::get_if<ActivityStarted>(&e)) live.push_back(a->activity);
        if (auto* a = std::get_if<AlertPosted>(&e)) alerts.push_back(a->alert);
        if (auto* d = std::get_if<DialogShown>(&e)) asked.push_back(d->dialog);
    }
    void act(const std::string& action) { registry.route({ "pomodoro", PomodoroProvider::kPanelId, action, 0 }); }
    void pass(Ms time) { now += time; wall += time; }
    void wake()
    {
        if (auto at = registry.nextWake()) pass(*at - now);
        registry.wakeDue(now);
    }
    // A whole session: each phase to its end, or skipped where `skip` says (by phase index).
    void session(std::initializer_list<int> skip = {})
    {
        act(pomodoro_action::kStart);
        for (int phase = 0; phase < 2 * static_cast<int>(tokens().pomodoroRounds); ++phase) {
            if (std::find(skip.begin(), skip.end(), phase) != skip.end()) {
                pass(Ms{ 60000 });
                act(pomodoro_action::kSkip);
            } else {
                wake();
            }
        }
    }
    PomodoroStats stats() { return loadStats(store[PomodoroProvider::kStatsKey]); }
    PomodoroDay today() { return countsOf(stats(), localDay(wall, Ms{ 0 })); }
    // Its window (decision 106), open from the start.
    void inWindow(const std::string& action) { registry.route({ "pomodoro", "", action, 0, "", PomodoroProvider::kWindowId }); }
    const WindowPage* page(const char* id)
    {
        const OpenWindow* w = findWindow(registry.windows(), { "pomodoro", PomodoroProvider::kWindowId });
        if (!w) return nullptr;
        for (const WindowPage& p : w->content.pages)
            if (p.id == id) return &p;
        return nullptr;
    }
    // Items as words: texts (a name, " · " and its description), "[Record]" for a tag, "fire" for the icon.
    static std::string words(const std::vector<Item>& items)
    {
        std::string out;
        auto add = [&](const std::string& word) {
            if (word.empty()) return;
            if (!out.empty()) out += " ";
            out += word;
        };
        for (const Item& item : items) {
            if (auto* text = std::get_if<Text>(&item)) add(text->value + (text->detail.empty() ? "" : " · " + text->detail));
            if (auto* tag = std::get_if<Tag>(&item)) add("[" + tag->text + "]");
            if (auto* icon = std::get_if<Icon>(&item)) add(icon->glyph == Glyph::Fire ? "fire" : "icon");
            if (auto* button = std::get_if<Button>(&item)) add(button->label);
        }
        return out;
    }
    // A page as lines: each section's title, then its rows as words.
    std::vector<std::string> lines(const char* id)
    {
        std::vector<std::string> out;
        if (const WindowPage* p = page(id))
            for (const WindowSection& section : p->sections) {
                if (!section.title.empty()) out.push_back(section.title);
                for (const Row& r : section.rows) out.push_back(words(r.items));
            }
        return out;
    }
    const WindowSection* section(const char* pageId, const std::string& title)
    {
        if (const WindowPage* p = page(pageId))
            for (const WindowSection& s : p->sections)
                if (s.title == title) return &s;
        return nullptr;
    }
    // The timer page's Today: cycles and focus time, then days in a row (a fire after it from three on).
    std::string line()
    {
        const WindowSection* today = section(kPomodoroTimerPage, "Today");
        return today && !today->rows.empty() ? words(today->rows[0].items) : "";
    }
    std::string corner()
    {
        const WindowSection* today = section(kPomodoroTimerPage, "Today");
        if (!today || today->rows.size() < 2) return "";
        std::string fire = words(today->rows[1].trailing);
        return (fire.empty() ? "" : fire + " ") + words(today->rows[1].items);
    }
    std::vector<std::string> statsPage() { return lines(kPomodoroStatsPage); }
    // The statistics page's table rows as words: today's values, what they are (or "[Record]"), the records.
    std::string today3() { return statsPage()[1]; }
    std::string todayCaptions() { return statsPage()[2]; }
    std::string records() { return statsPage()[4]; }
};

void providerTests()
{
    const Tokens& t = tokens();
    Timer timer;
    expect(timer.line() == "0 cycles · 0 min of focus" && timer.corner().empty(), "a first run: today's values, no days in a row shown");
    auto first = timer.statsPage();
    expect(first.size() == 7 && first[0] == "Today" && first[1] == "0 0m 0" && first[2] == "cycles focus time days in a row" &&
           first[3] == "🏆 Personal records" && first[4] == "0 0m 0 days" && first[5] == "most cycles best focus day best streak" &&
           first[6] == "Reset statistics · Every day before today is cleared.",
           "the statistics page: today, the records (each a value over its caption), then Reset");

    timer.session();
    PomodoroDay day = timer.today();
    expect(day.started == 1 && day.finished == 1 && day.focusDone == static_cast<int>(t.pomodoroRounds) && day.focusSkipped == 0,
           "a whole session: started, finished, every focus done");
    expect(day.focus == static_cast<long long>(t.pomodoroRounds) * t.pomodoroFocus, "and the focus time");
    expect(!timer.alerts.empty() && timer.alerts.back().content.label == "Session done" &&
           std::get<Text>(timer.alerts.back().content.alertCenter[1].items[0]).value == "1 finished today", "the last alert says how many today");
    expect(timer.line() == "1 cycle · 1 h 40 min of focus" && timer.corner() == "1 day in a row", "today: its values, the run");
    expect(timer.today3() == "1 1h 40m 1" && timer.todayCaptions() == "[Record] [Record] [Record]" && timer.records() == "1 1h 40m 1 day",
           "the page: records tagged, and the new records below");

    // One focus phase skipped still counts; two don't; a break skipped doesn't matter.
    timer.session({ 2, 3 });
    day = timer.today();
    expect(day.finished == 2 && day.focusSkipped == 1 && day.breaksSkipped == 1, "one focus skipped: still finished");
    timer.session({ 0, 2 });
    day = timer.today();
    expect(day.finished == 2 && day.started == 3 && day.focusSkipped == 3, "two focus phases skipped: not finished, not canceled");
    expect(day.canceled == 0, "nor canceled");

    // Stopped: canceled, its focus so far counted.
    Ms before = day.focus;
    timer.act(pomodoro_action::kStart);
    timer.pass(Ms{ 120000 });
    timer.act(pomodoro_action::kStopConfirmed);
    day = timer.today();
    expect(day.canceled == 1 && day.focus == before + Ms{ 120000 }, "stopped: canceled, two minutes of focus kept");

    // To-dos ticked count; unticked the same day, taken back.
    timer.registry.route({ "pomodoro", PomodoroProvider::kPanelId, todo_action::kAdd, 0, "write tests" });
    std::string toggle = std::string(todo_action::kToggle) + "1"; // the first to-do of the run
    timer.act(toggle);
    expect(timer.today().todosDone == 1, "a to-do ticked");
    timer.act(toggle);
    expect(timer.today().todosDone == 0, "unticked: taken back");

    // The next day: today starts over; the window follows once the card is shown again.
    timer.pass(Ms{ kDay });
    timer.registry.cardShown({ "pomodoro", PomodoroProvider::kPanelId });
    expect(timer.line() == "0 cycles · 0 min of focus" && timer.corner() == "1 day in a row", "a new day: none today yet, the run still on");
    expect(timer.today3().rfind("0 0m", 0) == 0 && timer.records().rfind("2 ", 0) == 0 && timer.todayCaptions() == "cycles focus time [Record]",
           "yesterday's 2 is the best; the run of 1 is still a record");
    timer.session();
    expect(timer.corner() == "2 days in a row" && timer.today3().rfind("1 ", 0) == 0 && timer.todayCaptions() == "cycles focus time [Record]",
           "two days in a row: a record; 1 today isn't");

    // A run broken: the old one is the best to beat; from three days on, a fire.
    timer.pass(Ms{ 2 * kDay });
    timer.registry.cardShown({ "pomodoro", PomodoroProvider::kPanelId });
    expect(timer.corner().empty() && timer.today3() == "0 0m 0" && timer.records().find("2 days") != std::string::npos,
           "a day missed: no run shown, best 2");
    for (int day = 0; day < 3; ++day) {
        timer.session();
        if (day < 2) timer.pass(Ms{ kDay });
    }
    expect(timer.corner() == "fire 3 days in a row" && timer.today3().find("🔥 3") != std::string::npos && timer.todayCaptions().find("[Record]") != std::string::npos,
           "three days: a fire, and a record");
}

// The statistics as laid out in the window: tables of equal columns, each value over its caption.
// And a page within a card keeps the card's height (Content::sizedLike, decision 070).
void layoutTests()
{
    const Tokens& t = tokens();
    Timer timer;
    timer.registry.windowPageShown({ "pomodoro", PomodoroProvider::kWindowId }, kPomodoroStatsPage);
    WindowScene page = windowLayout(*findWindow(timer.registry.windows(), { "pomodoro", PomodoroProvider::kWindowId }), { 1000, 900 }, 0, Ms{ 0 });
    std::vector<const Placed*> values, captions;
    for (const Placed& p : page.page)
        if (auto* text = std::get_if<Text>(&p.item)) {
            if (text->value == "0" || text->value == "0m") values.push_back(&p);
            if (text->value == "cycles" || text->value == "focus time") captions.push_back(&p);
        }
    expect(values.size() >= 3 && captions.size() == 2 && std::abs(values[0]->frame.width - values[1]->frame.width) < 0.01f &&
           captions[0]->frame.x == values[0]->frame.x && captions[1]->frame.x == values[1]->frame.x && captions[0]->frame.y > values[0]->frame.y,
           "equal columns: each value over its caption");

    auto shown = [](const Panel& panel) {
        IslandState state;
        state = reduce(state, PanelPublished{ panel }, Ms{ 0 });
        state = reduce(state, PillClicked{}, Ms{ 0 });
        state = reduce(state, TileClicked{ panel.id }, Ms{ 0 });
        return layout(state, Ms{ 0 });
    };
    auto at = [](const Scene& scene, const std::string& key) -> const Placed* {
        for (const Placed& p : scene.elements)
            if (p.key == key) return &p;
        return nullptr;
    };
    Panel tall{ .id = "tall", .content = { .label = "Tall", .card = CardLayout::Centred, .tile = { row({ Text{ "Tall" } }) } } };
    for (int i = 0; i < 6; ++i) tall.content.expandedCenter.push_back(row({ Text{ "line " + std::to_string(i), TextStyle::Title } }));
    Scene main = shown(tall);
    tall.content.sizedLike = tall.content.expandedCenter;
    tall.content.pageAtTop = true;
    tall.content.expandedCenter = { row({ Text{ "Title" } }) };
    for (int i = 0; i < 30; ++i) {
        Row r = row({ Text{ "row " + std::to_string(i) } });
        r.list = "long";
        if (i == 0) r.visibleRows = -1;
        tall.content.expandedCenter.push_back(r);
    }
    Scene paged = shown(tall);
    const Placed* firstRow = at(paged, "expandedCenter/1/0");
    float contentTop = paged.tabs ? paged.tabs->tabs[0].frame.y + t.tabHeight + t.slotGap : t.expandedPadding;
    expect(paged.size.y == main.size.y, "a page: the card's height, not its own");
    expect(firstRow && firstRow->list && firstRow->list->max > 0 &&
           std::abs(firstRow->list->viewport.y + firstRow->list->viewport.height - (paged.size.y - t.expandedPadding)) < 0.51f &&
           at(paged, "expandedCenter/0/0")->frame.y < contentTop + 1, "from the top, its list filling the rest, scrolling");
}

// Reset statistics (decision 071): asks first; confirmed, every day but today goes.
void resetTests()
{
    Timer timer;
    timer.session();
    timer.pass(Ms{ kDay });
    timer.session();
    timer.session();
    std::size_t days = timer.stats().days.size();
    timer.act(pomodoro_action::kResetStats);
    expect(timer.asked.size() == 1 && timer.asked.back().confirmAction == pomodoro_action::kResetStatsConfirmed && timer.asked.back().destructive &&
           timer.stats().days.size() == days && days == 2, "Reset asks first, and clears nothing yet");
    timer.act(pomodoro_action::kResetStatsConfirmed);
    PomodoroStats left = timer.stats();
    expect(left.days.size() == 1 && left.days[0].day == localDay(timer.wall, Ms{ 0 }) && left.days[0].finished == 2,
           "confirmed: only today stays, its count whole");
    expect(timer.records() == "2 3h 20m 1 day", "the records start over from today");

    // In the window it asks in place: Cancel keeps everything, Reset clears.
    timer.pass(Ms{ kDay });
    timer.session();
    timer.inWindow(pomodoro_action::kResetStats);
    auto asking = timer.statsPage();
    expect(timer.asked.size() == 1 && asking.back() == "Cancel Reset" && asking[asking.size() - 3] == "Reset statistics?", "the window asks in place");
    timer.inWindow(pomodoro_action::kCancel);
    expect(timer.stats().days.size() == 2 && timer.statsPage().back().rfind("Reset statistics", 0) == 0, "cancelled: nothing cleared");
    timer.inWindow(pomodoro_action::kResetStats);
    timer.inWindow(pomodoro_action::kResetStatsConfirmed);
    expect(timer.stats().days.size() == 1 && timer.statsPage().back().rfind("Reset statistics", 0) == 0, "confirmed: cleared, the question gone");
}

// Debug builds: the settings page's row steps through made-up histories, never saved.
void fakeStatsTests()
{
    Timer plain;
    expect(plain.lines(kPomodoroSettingsPage).back() != "Debug" && !plain.section(kPomodoroSettingsPage, "Debug"), "Release: no debug row");
    std::string before = plain.store[PomodoroProvider::kStatsKey];
    auto shown = plain.statsPage();
    plain.inWindow(pomodoro_action::kFakeStats);
    expect(plain.statsPage() == shown, "and its action does nothing");

    Timer timer(/* debug */ true);
    timer.session();
    std::string real = timer.store[PomodoroProvider::kStatsKey];
    expect(timer.lines(kPomodoroSettingsPage).back() == "Fake statistics · Real", "Debug: a Fake statistics row");
    timer.inWindow(pomodoro_action::kFakeStats);
    expect(timer.corner() == "fire 3 days in a row" && timer.today3() == "2 3h 20m 🔥 3" && timer.todayCaptions() == "cycles focus time days in a row" &&
           timer.records() == "4 6h 40m 5 days", "fake 1: records to beat, a fire");
    timer.act(pomodoro_action::kFakeStats);
    expect(timer.corner() == "fire 6 days in a row" && timer.today3() == "5 8h 20m 🔥 6" && timer.todayCaptions() == "[Record] [Record] [Record]",
           "fake 2: records");
    timer.session();
    expect(timer.store[PomodoroProvider::kStatsKey] == real, "made up: never saved");
    timer.act(pomodoro_action::kFakeStats);
    expect(timer.line() == "1 cycle · 1 h 40 min of focus", "the third click: the real statistics again");
}

// The timer's settings, a page of its window (decisions 061, 106): the plan, then the days off.
void settingsPageTests()
{
    Timer timer;
    const Row& top = timer.panels.back().content.expandedTrailing[0];
    expect(top.items.size() == 1 && std::get<Button>(top.items[0]).actionId == pomodoro_action::kOpenWindow,
           "the card's corner: only the expand button");
    auto texts = timer.lines(kPomodoroSettingsPage);
    const WindowSection* off = timer.section(kPomodoroSettingsPage, "Days off");
    expect(texts.size() == 6 && texts[0] == "Plan" && texts[1].rfind("Focus", 0) == 0 && texts[2].rfind("Break", 0) == 0 && texts[3] == "Days off" &&
           off && off->rows[0].items.size() == 7, "the page: the plan, then one row of seven round days");
    auto day = [&](std::size_t i) { return std::get<Button>(timer.section(kPomodoroSettingsPage, "Days off")->rows[0].items[i]); };
    expect(day(0).label == "Mo" && day(6).label == "Su" && day(0).round && day(0).on == false, "Monday first, none off");

    timer.inWindow(std::string(pomodoro_action::kToggleDayOff) + "5");
    expect(day(5).on == true && day(4).on == false, "Saturday off: lit");
    expect(loadDaysOff(timer.store[PomodoroProvider::kDaysOffKey])[5], "kept in the store");
    auto before = timer.lines(kPomodoroSettingsPage);
    timer.inWindow(std::string(pomodoro_action::kToggleDayOff) + "9");
    expect(timer.lines(kPomodoroSettingsPage) == before, "no such day: nothing changes");

    // Running: the plan says it waits; the − and + are dimmed.
    timer.act(pomodoro_action::kStart);
    const WindowSection* plan = timer.page(kPomodoroSettingsPage) ? &timer.page(kPomodoroSettingsPage)->sections[0] : nullptr;
    expect(plan && plan->title.rfind("Plan · ", 0) == 0 && !std::get<Button>(plan->rows[0].trailing[0]).enabled, "running: the plan dimmed, and says why");
}

} // namespace

void runPomodoroStatsTests()
{
    pureTests();
    providerTests();
    fakeStatsTests();
    resetTests();
    layoutTests();
    settingsPageTests();
}

} // namespace cup::test
