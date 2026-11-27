#include "providers/statistics/stats_network.h"

namespace cup {
namespace {

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

Traffic trafficOf(const AdapterSample& adapter, const std::vector<AdapterSample>& before)
{
    const AdapterSample* was = find(before, adapter.id);
    if (!was) return {};
    return { since(was->received, adapter.received), since(was->sent, adapter.sent) };
}

Traffic trafficSince(const std::vector<AdapterSample>& before, const std::vector<AdapterSample>& now)
{
    Traffic all;
    for (const AdapterSample& adapter : now) {
        Traffic one = trafficOf(adapter, before);
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

} // namespace cup
