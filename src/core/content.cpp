#include "core/content.h"

#include <algorithm>
#include <cmath>
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

Timeline levelTimeline(float level)
{
    constexpr long long kWhole = 1000; // any length: only the fraction is shown
    return { .position = Ms{ std::llround(std::clamp(level, 0.0f, 1.0f) * kWhole) }, .duration = Ms{ kWhole } };
}

namespace {

// The first line that starts after `position`: the one shown is the one before it.
std::size_t nextLine(const std::vector<LyricLine>& lines, Ms position)
{
    auto after = std::upper_bound(lines.begin(), lines.end(), position, [](Ms p, const LyricLine& line) { return p < line.at; });
    return static_cast<std::size_t>(after - lines.begin());
}

} // namespace

std::string lyricAt(const Lyrics& lyrics, Ms now)
{
    if (!lyrics.lines || lyrics.lines->empty()) return {}; // none yet: the row waits, empty
    std::size_t next = nextLine(*lyrics.lines, lyrics.timeline.positionAt(now));
    const std::string& text = next == 0 ? std::string() : (*lyrics.lines)[next - 1].text;
    return text.empty() ? kLyricBreak : text;
}

std::optional<Ms> nextLyricChange(const Lyrics& lyrics, Ms now)
{
    const Timeline& t = lyrics.timeline;
    if (!lyrics.lines || !t.playing || t.rate <= 0) return std::nullopt;
    Ms position = t.positionAt(now);
    std::size_t next = nextLine(*lyrics.lines, position);
    if (next >= lyrics.lines->size()) return std::nullopt;
    Ms at = (*lyrics.lines)[next].at;
    if (t.duration > Ms{ 0 } && at > t.duration) return std::nullopt; // past the end: never reached
    return now + Ms{ static_cast<long long>(std::ceil(static_cast<double>((at - position).count()) / t.rate)) };
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

std::string formatClock(Ms timeOfDay, bool hour24)
{
    long long minutes = timeOfDay.count() / 60000;
    long long hour = minutes / 60 % 24, minute = minutes % 60;
    if (!hour24) hour = hour % 12 == 0 ? 12 : hour % 12;
    return std::to_string(hour) + (minute < 10 ? ":0" : ":") + std::to_string(minute);
}

bool hasCard(const Content& c)
{
    return !c.expandedLeading.empty() || !c.expandedCenter.empty() || !c.expandedAside.empty() || !c.expandedTrailing.empty() || !c.expandedBottom.empty();
}

} // namespace cup
