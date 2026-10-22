#include "providers/pomodoro/pomodoro_stats.h"

#include "core/tokens.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sstream>

namespace cup {
namespace {

constexpr long long kDayMs = 86'400'000;

long long floorDiv(long long a, long long b) { return a / b - (a % b < 0 ? 1 : 0); }

std::string dateOf(long long day)
{
    std::chrono::year_month_day date{ std::chrono::sys_days{ std::chrono::days{ day } } };
    char text[16];
    std::snprintf(text, sizeof text, "%04d-%02u-%02u", static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
                  static_cast<unsigned>(date.day()));
    return text;
}

bool parseDate(const std::string& text, long long& day)
{
    int y = 0;
    unsigned m = 0, d = 0;
    char extra = 0;
    if (std::sscanf(text.c_str(), "%d-%u-%u%c", &y, &m, &d, &extra) != 3) return false;
    std::chrono::year_month_day date{ std::chrono::year{ y }, std::chrono::month{ m }, std::chrono::day{ d } };
    if (!date.ok()) return false;
    day = std::chrono::sys_days{ date }.time_since_epoch().count();
    return true;
}

} // namespace

long long localDay(Ms wallClock, Ms utcOffset) { return floorDiv(wallClock.count() + utcOffset.count(), kDayMs); }

Ms untilMidnight(Ms wallClock, Ms utcOffset)
{
    long long local = wallClock.count() + utcOffset.count();
    return Ms{ (floorDiv(local, kDayMs) + 1) * kDayMs - local };
}

int weekday(long long day) { return static_cast<int>(((day + 3) % 7 + 7) % 7); } // 1970-01-01 was a Thursday

PomodoroDay& dayOf(PomodoroStats& stats, long long day)
{
    auto at = std::lower_bound(stats.days.begin(), stats.days.end(), day, [](const PomodoroDay& d, long long v) { return d.day < v; });
    if (at != stats.days.end() && at->day == day) return *at;
    return *stats.days.insert(at, PomodoroDay{ .day = day });
}

PomodoroDay countsOf(const PomodoroStats& stats, long long day)
{
    for (const PomodoroDay& d : stats.days)
        if (d.day == day) return d;
    return { .day = day };
}

bool sessionCounts(int focusSkipped) { return focusSkipped >= 0 && static_cast<std::size_t>(focusSkipped) <= tokens().pomodoroFocusSkipsAllowed; }

Streaks streaksOf(const PomodoroStats& stats, const DaysOff& off, long long today)
{
    // Oldest to today; a gap between kept days is days with nothing in them.
    Streaks streaks;
    if (stats.days.empty()) return streaks;
    auto it = stats.days.begin();
    for (long long day = stats.days.front().day; day <= today; ++day) {
        bool kept = it != stats.days.end() && it->day == day;
        bool worked = kept && it->finished > 0;
        if (kept) ++it;
        if (worked) ++streaks.current;
        else if (day != today && !off[weekday(day)]) {
            streaks.bestBefore = std::max(streaks.bestBefore, streaks.current);
            streaks.current = 0;
        }
    }
    return streaks;
}

int daysInARow(const PomodoroStats& stats, const DaysOff& off, long long today) { return streaksOf(stats, off, today).current; }

int bestDay(const PomodoroStats& stats, long long except)
{
    int best = 0;
    for (const PomodoroDay& d : stats.days)
        if (d.day != except) best = std::max(best, d.finished);
    return best;
}

PomodoroDay totals(const PomodoroStats& stats)
{
    PomodoroDay all;
    for (const PomodoroDay& d : stats.days) {
        all.focus += d.focus;
        all.started += d.started;
        all.finished += d.finished;
        all.canceled += d.canceled;
        all.focusDone += d.focusDone;
        all.focusSkipped += d.focusSkipped;
        all.breaksSkipped += d.breaksSkipped;
        all.todosDone += d.todosDone;
    }
    return all;
}

PomodoroStats fakeStats(long long today, int which)
{
    PomodoroStats stats;
    if (which != 1 && which != 2) return stats;
    auto finished = [&](long long day, int count) {
        PomodoroDay& d = dayOf(stats, day);
        d.started = d.finished = count;
        d.focusDone = count * static_cast<int>(tokens().pomodoroRounds);
        d.focus = d.focusDone * tokens().pomodoroFocus;
    };
    for (long long day = today - 14; day <= today - 10; ++day) finished(day, day == today - 12 ? 4 : 1); // the old run: 5 days
    int run = which == 1 ? 3 : 6;
    for (long long day = today - run + 1; day <= today; ++day) finished(day, day == today ? (which == 1 ? 2 : 5) : 1);
    return stats;
}

std::string saveStats(const PomodoroStats& stats)
{
    std::ostringstream out;
    out << "stats1\n";
    for (const PomodoroDay& d : stats.days)
        out << dateOf(d.day) << ' ' << d.focus.count() << ' ' << d.started << ' ' << d.finished << ' ' << d.canceled << ' ' << d.focusDone
            << ' ' << d.focusSkipped << ' ' << d.breaksSkipped << ' ' << d.todosDone << '\n';
    return out.str();
}

PomodoroStats loadStats(const std::string& saved)
{
    std::istringstream in(saved);
    std::string line, tag;
    PomodoroStats stats;
    if (!std::getline(in, line) || !(std::istringstream(line) >> tag) || tag != "stats1") return {};
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string date;
        long long focus = 0;
        PomodoroDay d;
        if (!(fields >> date >> focus >> d.started >> d.finished >> d.canceled >> d.focusDone >> d.focusSkipped >> d.breaksSkipped >> d.todosDone))
            continue;
        if (!parseDate(date, d.day) || focus < 0) continue;
        d.focus = Ms{ focus };
        dayOf(stats, d.day) = d; // in order, whatever the file's
    }
    return stats;
}

std::string saveDaysOff(const DaysOff& off)
{
    std::string flags;
    for (bool day : off) flags += day ? '1' : '0';
    return "daysoff1 " + flags + "\n";
}

DaysOff loadDaysOff(const std::string& saved)
{
    std::istringstream in(saved);
    std::string tag, flags;
    DaysOff off{};
    if (!(in >> tag >> flags) || tag != "daysoff1" || flags.size() != off.size()) return {};
    for (std::size_t i = 0; i < off.size(); ++i) off[i] = flags[i] == '1';
    return off;
}

} // namespace cup
