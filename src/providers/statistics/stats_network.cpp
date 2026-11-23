#include "providers/statistics/stats_network.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace cup {
namespace {

// A boot time is worked out from two clocks (now, minus the time since
// boot), so it wobbles when Windows corrects its clock: the same boot within this many minutes.
constexpr std::int64_t kSameBootMinutes = 10;

const AdapterSample* find(const std::vector<AdapterSample>& adapters, std::uint64_t id)
{
    for (const AdapterSample& a : adapters)
        if (a.id == id) return &a;
    return nullptr;
}

std::uint64_t since(std::uint64_t before, std::uint64_t now)
{
    return now >= before ? now - before : now; // lower than before: it began again
}

} // namespace

Traffic trafficOf(const AdapterSample& adapter, const std::vector<AdapterSample>& before, bool restarted)
{
    if (restarted) return { adapter.received, adapter.sent };
    const AdapterSample* was = find(before, adapter.id);
    if (!was) return {};
    return { since(was->received, adapter.received), since(was->sent, adapter.sent) };
}

Traffic trafficSince(const std::vector<AdapterSample>& before, const std::vector<AdapterSample>& now, bool restarted)
{
    Traffic all;
    for (const AdapterSample& adapter : now) {
        Traffic one = trafficOf(adapter, before, restarted);
        all.received += one.received;
        all.sent += one.sent;
    }
    return all;
}

std::optional<float> perSecond(std::uint64_t bytes, double elapsedMs)
{
    if (!(elapsedMs > 0)) return std::nullopt;
    return static_cast<float>(static_cast<double>(bytes) * 1000.0 / elapsedMs);
}

const AdapterSample* adapterInUse(const std::vector<AdapterSample>& now, const std::vector<AdapterSample>& before, std::optional<std::uint64_t> lastId)
{
    const AdapterSample* best = nullptr;
    std::uint64_t bestMoved = 0;
    for (const AdapterSample& adapter : now) {
        if (!adapter.up) continue;
        Traffic traffic = trafficOf(adapter, before);
        std::uint64_t moved = traffic.received + traffic.sent;
        bool better = !best || moved > bestMoved;
        if (best && moved == bestMoved) {
            bool wasLast = lastId && adapter.id == *lastId, bestWasLast = lastId && best->id == *lastId;
            better = wasLast || (!bestWasLast && adapter.wired && !best->wired);
        }
        if (!better) continue;
        best = &adapter;
        bestMoved = moved;
    }
    return best;
}

NetworkDay advanceDay(const NetworkDay& kept, std::int64_t day, std::int64_t bootMinute, bool bootedToday, const std::vector<AdapterSample>& now)
{
    NetworkDay next{ .day = day, .bootMinute = bootMinute };
    for (const AdapterSample& adapter : now) next.last.push_back(AdapterSample{ .id = adapter.id, .received = adapter.received, .sent = adapter.sent });
    if (kept.day != day) {
        if (bootedToday) next.total = trafficSince({}, now, true);
        return next;
    }
    bool restarted = std::llabs(bootMinute - kept.bootMinute) > kSameBootMinutes;
    Traffic more = trafficSince(kept.last, now, restarted);
    next.total = { kept.total.received + more.received, kept.total.sent + more.sent };
    return next;
}

std::string toText(const NetworkDay& day)
{
    std::ostringstream out;
    out << "1 " << day.day << ' ' << day.bootMinute << ' ' << day.total.received << ' ' << day.total.sent;
    for (const AdapterSample& adapter : day.last) out << ' ' << adapter.id << ' ' << adapter.received << ' ' << adapter.sent;
    return out.str();
}

NetworkDay networkDayFromText(const std::string& text)
{
    std::istringstream in(text);
    int version = 0;
    NetworkDay day;
    if (!(in >> version >> day.day >> day.bootMinute >> day.total.received >> day.total.sent) || version != 1 || day.day <= 0) return {};
    AdapterSample adapter;
    while (in >> adapter.id >> adapter.received >> adapter.sent) day.last.push_back(adapter);
    if (!in.eof()) return {}; // something that isn't a number: not ours
    return day;
}

} // namespace cup
