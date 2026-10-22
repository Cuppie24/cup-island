#pragma once
// Pomodoro's statistics (decision 061): what was done each day, kept as one
// line per day in the host's store. Shown: sessions finished today and days
// in a row, each with its best, and focus time today; the rest is kept for later.
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
// Worked out from the days kept, with today's days off.
struct Streaks {
    int current = 0;
    int bestBefore = 0; // the longest run that ended before the current one
    bool operator==(const Streaks&) const = default;
};
Streaks streaksOf(const PomodoroStats& stats, const DaysOff& off, long long today);
int daysInARow(const PomodoroStats& stats, const DaysOff& off, long long today); // streaksOf(...).current
// The most sessions finished in one day, leaving out `except` (today, for a record).
int bestDay(const PomodoroStats& stats, long long except = -1'000'000);
PomodoroDay totals(const PomodoroStats& stats); // every day added up (day = 0)

// A made-up history for trying the card in Debug builds (decision 061):
// 1: a run of 3 up to today (best 5 before it), 2 finished today (best 4);
// 2: new records, a run of 6 and 5 finished today. Anything else: none.
PomodoroStats fakeStats(long long today, int which);

// "stats1", then "<yyyy-mm-dd> <focus ms> <started> <finished>
// <canceled> <focus done> <focus skipped> <breaks skipped> <to-dos done>" a
// day. Unreadable lines are skipped; nothing readable: no stats.
std::string saveStats(const PomodoroStats& stats);
PomodoroStats loadStats(const std::string& saved);
// "daysoff1 0000011" (Monday first; 1: off). Unreadable: no days off.
std::string saveDaysOff(const DaysOff& off);
DaysOff loadDaysOff(const std::string& saved);

} // namespace cup
