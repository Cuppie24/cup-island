// Pomodoro's statistics and its settings page (decision 061): days, days in
// a row with days off, what's kept, and the provider counting a real session.

#include "core/tokens.h"
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
    expect(bestDay(stats) == 3 && totals(stats).finished == 4 && totals(stats).todosDone == 2, "the best day, and totals");

    // Saved and loaded: every count, the dates readable.
    stats.bestStreak = 4;
    stats.days = { { .day = kMonday, .focus = Ms{ 6'000'000 }, .started = 3, .finished = 2, .canceled = 1, .focusDone = 7,
                     .focusSkipped = 1, .breaksSkipped = 2, .todosDone = 5 },
                   { .day = kMonday + 1, .finished = 1 } };
    std::string saved = saveStats(stats);
    expect(saved.find("2025-10-20 6000000 3 2 1 7 1 2 5\n") != std::string::npos, "a day per line, its date readable");
    expect(loadStats(saved) == stats, "saved and loaded");
    expect(loadStats("") == PomodoroStats{} && loadStats("junk") == PomodoroStats{}, "unreadable: no stats");
    PomodoroStats partly = loadStats("stats1 2\n2025-13-01 1 1 1 1 1 1 1 1\nnonsense\n2025-10-21 60000 1 1 0 4 0 0 0\n");
    expect(partly.bestStreak == 2 && partly.days.size() == 1 && partly.days[0].day == kMonday + 1, "unreadable lines skipped");

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
    ProviderRegistry registry;

    Timer()
        : registry([this](const Event& e, const char*) { record(e); }, [this] { return now; },
                   { .load = [this](const std::string&, const std::string& key) { return store[key]; },
                     .save = [this](const std::string&, const std::string& key, const std::string& text) { store[key] = text; },
                     .wallClock = [this] { return wall; },
                     .utcOffset = [] { return Ms{ 0 }; } })
    {
        registry.add(std::make_unique<PomodoroProvider>());
        registry.start("pomodoro");
    }

    void record(const Event& e)
    {
        if (auto* p = std::get_if<PanelPublished>(&e)) panels.push_back(p->panel);
        if (auto* a = std::get_if<ActivityStarted>(&e)) live.push_back(a->activity);
        if (auto* a = std::get_if<AlertPosted>(&e)) alerts.push_back(a->alert);
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
    // The dormant card's rows' first texts, top to bottom.
    std::vector<std::string> card(const Content& c)
    {
        std::vector<std::string> texts;
        for (const Row& r : c.expandedCenter)
            texts.push_back(!r.items.empty() && std::holds_alternative<Text>(r.items[0]) ? std::get<Text>(r.items[0]).value : "");
        return texts;
    }
    bool says(const std::string& text) { auto t = card(panels.back().content); return std::find(t.begin(), t.end(), text) != t.end(); }
};

void providerTests()
{
    const Tokens& t = tokens();
    Timer timer;
    expect(timer.says("0 finished today") && !timer.says("1 day in a row"), "a first run: none today, no days in a row yet");

    timer.session();
    PomodoroDay day = timer.today();
    expect(day.started == 1 && day.finished == 1 && day.focusDone == static_cast<int>(t.pomodoroRounds) && day.focusSkipped == 0,
           "a whole session: started, finished, every focus done");
    expect(day.focus == static_cast<long long>(t.pomodoroRounds) * t.pomodoroFocus, "and the focus time");
    expect(!timer.alerts.empty() && timer.alerts.back().content.label == "Session done" &&
           std::get<Text>(timer.alerts.back().content.alertCenter[1].items[0]).value == "1 finished today", "the last alert says how many today");
    expect(timer.says("1 finished today · best 1") && timer.says("1 day in a row · 1 h 40 min focused today"), "the dormant card says how it's going");

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

    // The next day: today starts over; the card follows once it's shown again.
    timer.pass(Ms{ kDay });
    timer.registry.cardShown({ "pomodoro", PomodoroProvider::kPanelId });
    expect(timer.says("0 finished today · best 2") && timer.says("1 day in a row"), "a new day: none today yet, the run still on");
    std::size_t published = timer.panels.size();
    timer.registry.cardShown({ "pomodoro", PomodoroProvider::kPanelId });
    expect(timer.panels.size() == published, "shown again the same day: nothing to change");
    timer.session();
    expect(timer.says("1 finished today · best 2") && timer.stats().bestStreak == 2, "two days in a row: the best run kept");
}

void settingsPageTests()
{
    Timer timer;
    const Content& dormant = timer.panels.back().content;
    const Row& top = dormant.expandedCenter[0];
    expect(top.trailing.size() == 1 && std::get<Button>(top.trailing[0]).glyph == Glyph::Settings &&
           std::get<Button>(top.trailing[0]).actionId == pomodoro_action::kOpenSettings, "a gear in the card's top-right corner");

    timer.act(pomodoro_action::kOpenSettings);
    Content page = timer.panels.back().content;
    auto texts = timer.card(page);
    expect(texts.size() == 10 && texts[0] == "Timer settings" && texts[1] == "Days off" && texts[2] == "Monday" && texts[8] == "Sunday",
           "the page: a title, then a switch per day, Monday first");
    expect(page.backAction == pomodoro_action::kBack && std::get<Button>(page.expandedCenter[0].leading[0]).actionId == pomodoro_action::kBack,
           "a back arrow; Esc goes back too");
    expect(!page.tile.empty(), "the widget stays as it was");

    timer.act(std::string(pomodoro_action::kToggleDayOff) + "5");
    page = timer.panels.back().content;
    expect(std::get<Switch>(page.expandedCenter[7].items[1]).on && !std::get<Switch>(page.expandedCenter[6].items[1]).on, "Saturday off");
    expect(loadDaysOff(timer.store[PomodoroProvider::kDaysOffKey])[5], "kept in the store");
    timer.act(std::string(pomodoro_action::kToggleDayOff) + "9");
    expect(timer.panels.back().content == page, "no such day: nothing changes");

    timer.act(pomodoro_action::kBack);
    expect(timer.card(timer.panels.back().content)[1] == "Focus", "back: the timer");

    // Running: the gear opens the same page over the session; closing the card leaves it.
    timer.act(pomodoro_action::kStart);
    timer.act(pomodoro_action::kOpenSettings);
    expect(timer.card(timer.live.back().content)[0] == "Timer settings" && !timer.live.back().content.compactCenter.empty(),
           "running: the page on the card, compact as it was");
    timer.registry.cardClosed({ "pomodoro", PomodoroProvider::kPanelId });
    expect(timer.card(timer.live.back().content)[0] != "Timer settings", "closed: it opens on the timer next time");
}

} // namespace

void runPomodoroStatsTests()
{
    pureTests();
    providerTests();
    settingsPageTests();
}

} // namespace cup::test
