#pragma once
// The network's numbers from adapters' byte counters (decision 173), pure:
// what went through since the last look, which adapter is the one in use,
// and today's totals, kept between runs as text (Host::save, decision 028).
//
// Windows counts each adapter's bytes since it started (the PC booted, or
// the adapter was plugged in or reset). A speed is the difference between
// two looks; today's total adds those differences up, across closed cards
// and restarts: the counters went on counting meanwhile.

#include "providers/statistics/stats_reading.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// A hardware adapter as Windows lists it (filters and virtual ones left out
// by the reader: they repeat their adapter's traffic).
struct AdapterSample {
    std::uint64_t id = 0;        // its LUID: the same from boot to boot
    std::string name;            // "Wi-Fi", "Ethernet 2": Windows' name for the connection
    std::uint64_t received = 0;  // bytes, since the adapter started
    std::uint64_t sent = 0;
    std::uint64_t linkSpeed = 0; // bits per second
    bool wired = false;          // Ethernet
    bool up = false;             // connected
    bool operator==(const AdapterSample&) const = default;
};

struct Traffic {
    std::uint64_t received = 0; // bytes
    std::uint64_t sent = 0;
    bool operator==(const Traffic&) const = default;
};

// What went through `adapter` since `before`'s look at it. An adapter that
// wasn't there before: nothing (its counter says nothing about this span),
// unless `restarted` (the PC booted since: every counter began at 0 then).
// A counter lower than before began again: all of it is new.
Traffic trafficOf(const AdapterSample& adapter, const std::vector<AdapterSample>& before, bool restarted = false);
// ...and through all of them together.
Traffic trafficSince(const std::vector<AdapterSample>& before, const std::vector<AdapterSample>& now, bool restarted = false);
// Bytes per second over `elapsedMs`; none for no time at all.
std::optional<float> perSecond(std::uint64_t bytes, double elapsedMs);

// The adapter in use: of those connected, the one most went through since
// `before`; with nothing moving, the one picked last time (`lastId`), so the
// name doesn't jump between Wi-Fi and a docked cable; else a cable before
// Wi-Fi. None connected: none.
const AdapterSample* adapterInUse(const std::vector<AdapterSample>& now, const std::vector<AdapterSample>& before, std::optional<std::uint64_t> lastId);

// Today's totals as kept between looks and runs.
struct NetworkDay {
    std::int64_t day = 0;            // the local date, as yyyymmdd; 0: nothing kept
    std::int64_t bootMinute = 0;     // when the PC booted, minutes (UTC): a later one means the counters began again
    Traffic total;                   // since local midnight (see advanceDay)
    std::vector<AdapterSample> last; // the counters at the last look (id, received and sent only)
    bool operator==(const NetworkDay&) const = default;
};
// One more look, on `day`. The same day: what went through since the last
// look is added. A new day (or nothing kept): the total starts again, at
// everything since boot if the PC booted today (`bootedToday`: that is all of
// today's), else at 0: what went through between midnight and this first
// look isn't known, since nothing is read while the card is closed. So on a
// PC up since yesterday, "today" is "since it was first looked at today".
NetworkDay advanceDay(const NetworkDay& kept, std::int64_t day, std::int64_t bootMinute, bool bootedToday, const std::vector<AdapterSample>& now);

// As text for the store, and back; anything unreadable is nothing kept.
std::string toText(const NetworkDay& day);
NetworkDay networkDayFromText(const std::string& text);

} // namespace cup
