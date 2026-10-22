#pragma once
// Pomodoro's statistics (decision 061): what was done each day, kept as one
// line per day in the host's store. Shown: sessions finished today, the best
// day, days in a row and focus time today; the rest is kept for later.
// Pure: days are passed in (localDay), so midnight and days off test on Linux.

#include "core/content.h"

#include <array>
#include <string>
#include <vector>

namespace cup {

// One local day's counts. A session is all the phases, focus to long break.
struct PomodoroDay {
    long long day = 0;     // days since 1970-01-01, local (localDay)
    Ms focus{};            // focus actually spent, pauses left out
    int started = 0;       // sessions started
    int finished = 0;      // ...ended after the long break, at most pomodoroFocusSkipsAllowed focus phases skipped
    int canceled = 0;      // ...stopped by the user
    int focusDone = 0;     // focus phases that ran to the end
    int focusSkipped = 0;
    int breaksSkipped = 0; // breaks and long breaks
    int todosDone = 0;     // to-dos ticked (unticked the same day: taken back)
    bool operator==(const PomodoroDay&) const = default;
};

struct PomodoroStats {
    std::vector<PomodoroDay> days; // oldest first; only days with something in them
    int bestStreak = 0;            // the most days in a row so far
    bool operator==(const PomodoroStats&) const = default;
};

// Days off (decision 061): they don't break days in a row, and count when worked.
// Monday first, as the settings page lists them.
using DaysOff = std::array<bool, 7>;

long long localDay(Ms wallClock, Ms utcOffset); // the day the user's clock shows
Ms untilMidnight(Ms wallClock, Ms utcOffset);   // when that day ends, from now
int weekday(long long day);                     // 0 Monday .. 6 Sunday

// `day`'s counts, added if it has none yet (and kept in order).
PomodoroDay& dayOf(PomodoroStats& stats, long long day);
PomodoroDay countsOf(const PomodoroStats& stats, long long day); // zeros if none

// A session ended after its long break with this many focus phases skipped: does it count as finished?
bool sessionCounts(int focusSkipped);

// Days in a row with a finished session up to `today`. Today without one
// yet doesn't break it, nor does a day off; a day off with one counts.
int daysInARow(const PomodoroStats& stats, const DaysOff& off, long long today);
int bestDay(const PomodoroStats& stats); // the most sessions finished in one day
PomodoroDay totals(const PomodoroStats& stats); // every day added up (day = 0)

// "stats1 <best streak>", then "<yyyy-mm-dd> <focus ms> <started> <finished>
// <canceled> <focus done> <focus skipped> <breaks skipped> <to-dos done>" a
// day. Unreadable lines are skipped; nothing readable: no stats.
std::string saveStats(const PomodoroStats& stats);
PomodoroStats loadStats(const std::string& saved);
// "daysoff1 0000011" (Monday first; 1: off). Unreadable: no days off.
std::string saveDaysOff(const DaysOff& off);
DaysOff loadDaysOff(const std::string& saved);

} // namespace cup
