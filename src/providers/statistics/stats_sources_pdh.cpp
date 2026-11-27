// Performance counters through PDH (Windows Vista+; decision 173): the one
// documented, unprivileged way to the GPU's load, per-core load, disk
// throughput and processes without opening each one.
#include "providers/statistics/stats_sources.h"

#include "platform/win.h"

#include <pdh.h>
#include <pdhmsg.h>

namespace cup::stats {
namespace {

// Never capped at 100: a process on four cores is 400 % of one, and the
// cleaning is stats_format's (decision 171).
constexpr DWORD kFormat = PDH_FMT_DOUBLE | PDH_FMT_NOCAP100;

bool valid(const PDH_FMT_COUNTERVALUE& value)
{
    return value.CStatus == PDH_CSTATUS_VALID_DATA || value.CStatus == PDH_CSTATUS_NEW_DATA;
}

} // namespace

CounterSet::CounterSet()
{
    PDH_HQUERY query = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS) query_ = query;
}

CounterSet::~CounterSet()
{
    if (query_) PdhCloseQuery(static_cast<PDH_HQUERY>(query_));
}

CounterSet::Counter CounterSet::add(const wchar_t* path)
{
    if (!query_) return nullptr;
    PDH_HCOUNTER counter = nullptr;
    // The English name: a counter's own name is translated with Windows.
    if (PdhAddEnglishCounterW(static_cast<PDH_HQUERY>(query_), path, 0, &counter) != ERROR_SUCCESS) return nullptr;
    return counter;
}

bool CounterSet::collect()
{
    return query_ && PdhCollectQueryData(static_cast<PDH_HQUERY>(query_)) == ERROR_SUCCESS;
}

std::optional<double> CounterSet::value(Counter counter) const
{
    if (!counter) return std::nullopt;
    PDH_FMT_COUNTERVALUE formatted{};
    if (PdhGetFormattedCounterValue(static_cast<PDH_HCOUNTER>(counter), kFormat, nullptr, &formatted) != ERROR_SUCCESS || !valid(formatted))
        return std::nullopt;
    return formatted.doubleValue;
}

std::vector<CounterValue> CounterSet::values(Counter counter, bool* read) const
{
    if (read) *read = false;
    std::vector<CounterValue> found;
    if (!counter) return found;
    // PDH says how many bytes it needs (the items, then their names), and
    // the instances can change between asking and reading: a few tries.
    std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> buffer;
    for (int attempt = 0; attempt < 3; ++attempt) {
        DWORD bytes = static_cast<DWORD>(buffer.size() * sizeof(PDH_FMT_COUNTERVALUE_ITEM_W));
        DWORD count = 0;
        PDH_STATUS status = PdhGetFormattedCounterArrayW(static_cast<PDH_HCOUNTER>(counter), kFormat, &bytes, &count, buffer.empty() ? nullptr : buffer.data());
        if (status == static_cast<PDH_STATUS>(PDH_MORE_DATA)) {
            buffer.assign(bytes / sizeof(PDH_FMT_COUNTERVALUE_ITEM_W) + 1, PDH_FMT_COUNTERVALUE_ITEM_W{});
            continue;
        }
        if (status != ERROR_SUCCESS) return found; // no second sample yet, or no such instances
        if (read) *read = true;
        for (DWORD i = 0; i < count && i < buffer.size(); ++i)
            if (valid(buffer[i].FmtValue)) found.push_back({ utf8(buffer[i].szName), buffer[i].FmtValue.doubleValue });
        return found;
    }
    return found;
}

} // namespace cup::stats
