#pragma once
// The network's numbers from adapters' byte counters (decision 173), pure:
// what went through since the last look, and which adapter is the one in use.
//
// Windows counts each adapter's bytes since it started (the PC booted, or
// the adapter was plugged in or reset). A speed is the difference between
// two looks. Totals "today" aren't kept: nothing is read with the card
// closed, so they'd be right only on a PC booted that day (decision 173).

#include "providers/statistics/stats_reading.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

// A hardware adapter as Windows lists it (filters and virtual ones left out
// by the reader: they repeat their adapter's traffic).
struct AdapterSample {
    std::uint64_t id = 0;        // its LUID
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
// wasn't there before: nothing (its counter says nothing about this span).
// A counter lower than before began again: all of it is new.
Traffic trafficOf(const AdapterSample& adapter, const std::vector<AdapterSample>& before);
// ...and through all of them together.
Traffic trafficSince(const std::vector<AdapterSample>& before, const std::vector<AdapterSample>& now);
// Bytes per second over `elapsedMs`; none for no time at all.
std::optional<float> perSecond(std::uint64_t bytes, double elapsedMs);

// The adapter in use: of those connected, the one most went through since
// `before`; with nothing moving, the one picked last time (`lastId`), so the
// name doesn't jump between Wi-Fi and a docked cable; else a cable before
// Wi-Fi. None connected: none.
const AdapterSample* adapterInUse(const std::vector<AdapterSample>& now, const std::vector<AdapterSample>& before, std::optional<std::uint64_t> lastId);

} // namespace cup
