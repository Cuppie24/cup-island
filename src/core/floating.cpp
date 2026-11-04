#include "core/floating.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {

bool opensFloating(const IslandState& s, const std::string& cardId)
{
    const Content* c = cardContent(s, cardId);
    return c && c->floating;
}

FloatingCard* floatFor(IslandState& s, const std::string& cardId)
{
    for (FloatingCard& f : s.floats)
        if (f.activityId == cardId) return &f;
    return nullptr;
}

const FloatingCard* floatFor(const IslandState& s, const std::string& cardId)
{
    for (const FloatingCard& f : s.floats)
        if (f.activityId == cardId) return &f;
    return nullptr;
}

const FloatingCard* focusedFloat(const IslandState& s)
{
    return s.focusedFloatId.empty() ? nullptr : floatFor(s, s.focusedFloatId);
}

bool openFloat(IslandState& s, const std::string& cardId)
{
    if (!opensFloating(s, cardId)) return false;
    FloatingCard* f = floatFor(s, cardId);
    if (!f) {
        const Activity* a = findActivity(s, cardId);
        s.floats.push_back({ cardId, a ? a->providerId : findPanel(s, cardId)->providerId });
        f = &s.floats.back();
    }
    // In front of the others, with the keys: what Windows will say once the
    // platform raises it (FloatFocused), set now so a key in between finds it.
    f->raised = s.nextRaise++;
    s.focusedFloatId = cardId;
    return true;
}

void closeFloat(IslandState& s, const std::string& cardId)
{
    std::erase_if(s.floats, [&](const FloatingCard& f) { return f.activityId == cardId; });
    if (s.focusedFloatId == cardId) s.focusedFloatId.clear();
}

void focusFloat(IslandState& s, const std::string& cardId) { s.focusedFloatId = floatFor(s, cardId) ? cardId : ""; }

void resizeFloat(IslandState& s, const std::string& cardId, Vec2 size)
{
    const Vec2 least = tokens().floatMinSize;
    if (FloatingCard* f = floatFor(s, cardId)) f->size = Vec2{ std::max(size.x, least.x), std::max(size.y, least.y) };
}

void settleFloats(IslandState& s, Ms now)
{
    // A card stays while its activity or its panel does: an activity ending
    // leaves its panel's dormant card floating, as on the island (decision 009).
    std::erase_if(s.floats, [&](const FloatingCard& f) { return !cardContent(s, f.activityId); });
    for (FloatingCard& f : s.floats)
        if (f.overlays.toast && now >= f.overlays.toast->until) f.overlays.toast.reset();
    if (s.hidden) s.focusedFloatId.clear(); // hidden with the island: nothing to type into
    if (!s.focusedFloatId.empty() && !floatFor(s, s.focusedFloatId)) s.focusedFloatId.clear();
}

CardOverlays* overlaysOf(IslandState& s, const std::string& cardId)
{
    if (FloatingCard* f = floatFor(s, cardId)) return &f->overlays;
    return !s.homeOpen && !cardId.empty() && s.expandedId == cardId ? &s.card : nullptr;
}

const CardOverlays* overlaysOf(const IslandState& s, const std::string& cardId)
{
    if (const FloatingCard* f = floatFor(s, cardId)) return &f->overlays;
    return !s.homeOpen && !cardId.empty() && s.expandedId == cardId ? &s.card : nullptr;
}

std::vector<ShownCard> shownCards(const IslandState& s)
{
    std::vector<ShownCard> cards;
    if (auto card = shownCard(s)) cards.push_back(*card);
    for (const FloatingCard& f : s.floats) cards.push_back({ f.providerId, f.activityId });
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
    const CardOverlays* o = card ? overlaysOf(s, card->activityId) : nullptr;
    return o && o->dialog ? &*o->dialog : nullptr;
}

bool cardOnScreen(const IslandState& s, const std::string& cardId) { return overlaysOf(s, cardId) != nullptr; }

bool floatsShown(const IslandState& s) { return !s.hidden; }

float floatWidth(FloatWidth width)
{
    const Tokens& t = tokens();
    switch (width) {
    case FloatWidth::Narrow: return t.floatNarrowWidth;
    case FloatWidth::Medium: return t.floatMediumWidth;
    case FloatWidth::Wide: return t.floatWideWidth;
    }
    return t.floatMediumWidth;
}

} // namespace cup
