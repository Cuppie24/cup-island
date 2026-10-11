#include "core/tabs.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

bool hasTabs(const IslandState& s) { return mode(s) == Mode::Expanded && s.activities.size() > 1; }

const Item* firstItem(const Slot& slot)
{
    for (const Row& row : slot)
        if (!row.items.empty()) return &row.items.front();
    return nullptr;
}

// Title: the compact centre's text (media's "Song - Artist"), else the
// activity's name. Ends in "…" when long: a row of swimming tabs would be busy.
Text titleOf(const Content& c)
{
    const Item* item = firstItem(c.compactCenter);
    if (auto* text = item ? std::get_if<Text>(item) : nullptr) return Text{ text->value, TextStyle::Body };
    return Text{ c.label, TextStyle::Body };
}

// Lead square and title, centred in the tab as one group (the title as wide
// as it really is, so the group's width is known). A title too long for the
// tab gets what's left and ends in "…". Keys carry the activity id, so a
// tab's content stays the same visual when tabs come and go.
void placeTab(const Activity& a, Rect tab, float radius, const MeasureText& measure, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    float inset = t.compactInset, side = tab.height - 2 * inset, gap = inset;
    auto place = [&](const std::string& part, Item item, Rect frame, float cornerRadius = 0) {
        out.push_back({ .key = "tab/" + a.id + "/" + part, .item = std::move(item), .frame = frame, .hitFrame = frame,
                        .cornerRadius = cornerRadius, .providerId = a.providerId, .activityId = a.id });
    };

    const Item* lead = firstItem(a.content.compactLeading);
    if (!lead) lead = firstItem(a.content.minimal);
    Text title = titleOf(a.content);
    float leadWidth = lead ? side + gap : 0;
    float room = std::max(0.0f, tab.width - 2 * t.itemSpacing - leadWidth); // side margins = item spacing
    float titleWidth = std::min(measure(title) + 1, room); // +1: rounding never trips the "…"
    float x = tab.x + (tab.width - (leadWidth + titleWidth)) / 2;

    if (lead) {
        if (std::holds_alternative<Image>(*lead)) {
            place("lead", *lead, { x, tab.y + inset, side, side }, std::max(0.0f, std::min(radius - inset, side / 2)));
        } else {
            float g = std::min(t.iconSize, side); // glyphs: icon-sized, centred in the square
            place("lead", *lead, { x + (side - g) / 2, tab.y + (tab.height - g) / 2, g, g });
        }
        x += leadWidth;
    }
    place("title", title, { x, tab.y, titleWidth, tab.height });
}

} // namespace

float tabStripSpace(const IslandState& s)
{
    return hasTabs(s) ? tokens().tabHeight + tokens().slotGap : 0;
}

std::optional<TabStrip> tabStripFor(const IslandState& s, Vec2 cardSize, float nestedRadius, const MeasureText& measure,
                                    std::vector<Placed>& elements)
{
    if (!hasTabs(s)) return std::nullopt;
    const Tokens& t = tokens();

    std::vector<const Activity*> order;
    for (const Activity& a : s.activities) order.push_back(&a);
    std::sort(order.begin(), order.end(), [](const Activity* a, const Activity* b) { return a->startOrder < b->startOrder; });

    // Every tab at its full width, the group centred; narrower only when they don't fit.
    float p = t.expandedPadding, available = cardSize.x - 2 * p, n = static_cast<float>(order.size());
    float width = std::min(t.tabMaxWidth, (available - (n - 1) * t.tabGap) / n);
    float left = (cardSize.x - (n * width + (n - 1) * t.tabGap)) / 2;

    TabStrip strip;
    strip.radius = std::min(nestedRadius, t.tabHeight / 2);
    for (std::size_t i = 0; i < order.size(); ++i) {
        Rect frame{ left + static_cast<float>(i) * (width + t.tabGap), cardSize.y - p - t.tabHeight, width, t.tabHeight };
        if (order[i]->id == s.expandedId) strip.active = i;
        strip.tabs.push_back({ frame, order[i]->id });
        placeTab(*order[i], frame, strip.radius, measure, elements);
    }
    return strip;
}

const Tab* tabAt(const TabStrip& strip, Vec2 point)
{
    for (const Tab& tab : strip.tabs)
        if (tab.frame.contains(point)) return &tab;
    return nullptr;
}

} // namespace cup
