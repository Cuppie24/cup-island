#include "core/content.h"

#include <algorithm>
#include <cstdio>

namespace cup {

Ms Timeline::positionAt(Ms now) const
{
    Ms p = position;
    if (playing) p += Ms{ static_cast<long long>(static_cast<double>((now - anchoredAt).count()) * rate) };
    if (duration > Ms{ 0 }) p = std::min(p, duration);
    return std::max(p, Ms{ 0 });
}

float Timeline::fractionAt(Ms now) const
{
    if (duration <= Ms{ 0 }) return 0.0f;
    return static_cast<float>(positionAt(now).count()) / static_cast<float>(duration.count());
}

Row row(std::initializer_list<Item> items, Align align)
{
    return Row{ std::vector<Item>(items), align };
}

std::string formatDuration(Ms duration)
{
    long long total = std::max<long long>(duration.count() / 1000, 0);
    long long h = total / 3600, m = (total / 60) % 60, s = total % 60;
    char buffer[32];
    if (h > 0) std::snprintf(buffer, sizeof buffer, "%lld:%02lld:%02lld", h, m, s);
    else std::snprintf(buffer, sizeof buffer, "%lld:%02lld", m, s);
    return buffer;
}

} // namespace cup
