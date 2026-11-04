#include "core/floating.h"

#include "core/dialog.h"
#include "core/tokens.h"

#include <algorithm>

namespace cup {

bool opensAsWindow(const IslandState& s, const std::string& cardId)
{
    const Content* c = cardContent(s, cardId);
    return c && c->window;
}

FloatingWindow* windowFor(IslandState& s, const std::string& cardId)
{
    for (FloatingWindow& w : s.windows)
        if (w.activityId == cardId) return &w;
    return nullptr;
}

const FloatingWindow* windowFor(const IslandState& s, const std::string& cardId)
{
    for (const FloatingWindow& w : s.windows)
        if (w.activityId == cardId) return &w;
    return nullptr;
}

const FloatingWindow* focusedWindow(const IslandState& s)
{
    return s.focusedWindow.empty() ? nullptr : windowFor(s, s.focusedWindow);
}

bool openWindow(IslandState& s, const std::string& cardId)
{
    const Content* content = cardContent(s, cardId);
    if (!content || !content->window) return false;
    FloatingWindow* w = windowFor(s, cardId);
    if (!w) {
        const Activity* a = findActivity(s, cardId);
        std::string provider = a ? a->providerId : findPanel(s, cardId)->providerId;
        auto open = std::count_if(s.windows.begin(), s.windows.end(), [&](const FloatingWindow& o) { return o.providerId == provider; });
        if (static_cast<std::size_t>(open) >= tokens().windowsPerProvider) return false;
        s.windows.push_back({ cardId, provider, windowSize(content->window->size) });
        w = &s.windows.back();
    }
    // Opened, or clicked again while open: to the front, with the keys. A
    // second click does nothing more (no hiding it again).
    w->raised = s.nextRaise++;
    s.focusedWindow = cardId;
    return true;
}

void closeWindow(IslandState& s, const std::string& cardId)
{
    std::erase_if(s.windows, [&](const FloatingWindow& w) { return w.activityId == cardId; });
    if (s.focusedWindow == cardId) s.focusedWindow.clear();
}

void focusWindow(IslandState& s, const std::string& cardId) { s.focusedWindow = windowFor(s, cardId) ? cardId : ""; }

void resizeWindow(IslandState& s, const std::string& cardId, Vec2 size)
{
    const Vec2 least = tokens().windowMinSize;
    if (FloatingWindow* w = windowFor(s, cardId)) w->size = { std::max(size.x, least.x), std::max(size.y, least.y) };
}

void settleWindows(IslandState& s, Ms now)
{
    // A card stays while its activity or its panel does: an activity ending
    // leaves its panel's dormant card in the window, as on the island (decision 009).
    std::erase_if(s.windows, [&](const FloatingWindow& w) { return !cardContent(s, w.activityId); });
    for (FloatingWindow& w : s.windows) {
        if (w.toast && now >= w.toast->until) w.toast.reset();
    }
    if (!s.focusedWindow.empty() && !windowFor(s, s.focusedWindow)) s.focusedWindow.clear();
}

std::vector<ShownCard> shownCards(const IslandState& s)
{
    std::vector<ShownCard> cards;
    if (auto card = shownCard(s)) cards.push_back(*card);
    for (const FloatingWindow& w : s.windows) cards.push_back({ w.providerId, w.activityId });
    return cards;
}

CardChanges cardChanges(const std::vector<ShownCard>& before, const std::vector<ShownCard>& after)
{
    auto has = [](const std::vector<ShownCard>& cards, const ShownCard& c) { return std::find(cards.begin(), cards.end(), c) != cards.end(); };
    CardChanges changes;
    for (const ShownCard& c : before)
        if (!has(after, c)) changes.closed.push_back(c);
    for (const ShownCard& c : after)
        if (!has(before, c)) changes.shown.push_back(c);
    return changes;
}

const Dialog* focusedDialog(const IslandState& s)
{
    auto card = focusedCard(s);
    return card ? dialogOver(s, card->activityId) : nullptr;
}

bool cardOnScreen(const IslandState& s, const std::string& cardId)
{
    return (!s.homeOpen && s.expandedId == cardId) || windowFor(s, cardId);
}

Vec2 windowSize(WindowSize size)
{
    const Tokens& t = tokens();
    switch (size) {
    case WindowSize::Small: return t.windowSmallSize;
    case WindowSize::Medium: return t.windowMediumSize;
    case WindowSize::Large: return t.windowLargeSize;
    }
    return t.windowMediumSize;
}

} // namespace cup
