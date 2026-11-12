#include "core/island.h"

// Read-only questions about the state: lookups, the clock, names for logs.
// Split from island.cpp, which keeps the rules (reduce).

namespace cup {
namespace {

constexpr Ms kDay{ 24 * 60 * 60 * 1000 };

} // namespace

bool isExpanded(const IslandState& s) { return s.homeOpen || !s.expandedId.empty(); }

const Activity* findActivity(const IslandState& s, const std::string& id)
{
    for (const auto& a : s.activities)
        if (a.id == id) return &a;
    return nullptr;
}

const Panel* findPanel(const IslandState& s, const std::string& id)
{
    for (const auto& p : s.panels)
        if (p.id == id) return &p;
    return nullptr;
}

std::optional<ShownCard> shownCard(const IslandState& s)
{
    if (!isExpanded(s) || s.homeOpen) return std::nullopt;
    if (const Activity* a = findActivity(s, s.expandedId)) return ShownCard{ a->providerId, a->id };
    if (const Panel* p = findPanel(s, s.expandedId)) return ShownCard{ p->providerId, p->id };
    return std::nullopt;
}

const Content* cardContent(const IslandState& s, const std::string& id)
{
    if (const Activity* a = findActivity(s, id)) return &a->content;
    if (const Panel* p = findPanel(s, id)) return &p->content;
    return nullptr;
}

std::optional<ShownCard> shownActivity(const IslandState& s)
{
    auto card = shownCard(s);
    if (!card || !findActivity(s, card->activityId)) return std::nullopt;
    return card;
}

std::string backAction(const IslandState& s)
{
    auto card = shownCard(s);
    const Content* c = card ? cardContent(s, card->activityId) : nullptr;
    return c ? c->backAction : "";
}

bool escapeCloses(const IslandState& s)
{
    auto card = shownCard(s);
    const Content* c = card ? cardContent(s, card->activityId) : nullptr;
    return c && !c->fieldFirst.empty();
}

std::optional<Ms> timeOfDay(const IslandState& s, Ms now)
{
    if (!s.clock) return std::nullopt;
    return ((now + s.clock->offset) % kDay + kDay) % kDay;
}

Ms nextMinute(Ms timeOfDay, Ms now) { return now + (Ms{ 60000 } - timeOfDay % Ms{ 60000 }); }

const char* toString(Mode mode)
{
    switch (mode) {
    case Mode::Idle: return "Idle";
    case Mode::Compact: return "Compact";
    case Mode::Split: return "Split";
    case Mode::Alert: return "Alert";
    case Mode::Expanded: return "Expanded";
    case Mode::Receptive: return "Receptive";
    case Mode::Hidden: return "Hidden";
    }
    return "?";
}

const char* toString(Level level)
{
    switch (level) {
    case Level::Passive: return "passive";
    case Level::Active: return "active";
    case Level::TimeSensitive: return "timeSensitive";
    case Level::Critical: return "critical";
    }
    return "?";
}

} // namespace cup
