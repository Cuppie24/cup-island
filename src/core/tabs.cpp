#include "core/tabs.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// Somewhere else to go: another activity, or Home (with panels) from any card.
bool hasTabs(const IslandState& s)
{
    if (mode(s) != Mode::Expanded) return false;
    bool home = !s.panels.empty();
    bool dormantCard = !s.homeOpen && !findActivity(s, s.expandedId);
    return s.activities.size() + (home ? 1 : 0) > 1 || (home && dormantCard);
}

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

// What a tab shows: a lead square (icon or art) and a title.
struct TabFace {
    std::string id; // "" = Home
    std::string providerId;
    const Item* lead = nullptr;
    Text title;
    bool iconOnly = false; // Home: the lead, centred, no title
};

TabFace faceOf(const Activity& a)
{
    const Item* lead = firstItem(a.content.compactLeading);
    if (!lead) lead = firstItem(a.content.minimal);
    return { a.id, a.providerId, lead, titleOf(a.content) };
}

const Item kHomeIcon = Icon{ Glyph::Home };

// Lead square and title, centred in the tab as one group (the title as wide
// as it really is, so the group's width is known). A title too long for the
// tab gets what's left and ends in "…". Keys carry the activity id, so a
// tab's content stays the same visual when tabs come and go.
void placeTab(const TabFace& face, Rect tab, float radius, const MeasureText& measure, std::vector<Placed>& out)
{
    const Tokens& t = tokens();
    float inset = t.compactInset, side = tab.height - 2 * inset, gap = inset;
    std::string prefix = face.id.empty() ? "tab/home/" : "tab/" + face.id + "/";
    auto place = [&](const std::string& part, Item item, Rect frame, float cornerRadius = 0) {
        out.push_back({ .key = prefix + part, .item = std::move(item), .frame = frame, .hitFrame = frame,
                        .cornerRadius = cornerRadius, .providerId = face.providerId, .activityId = face.id });
    };

    const Item* lead = face.lead;
    const Text& title = face.title;
    if (face.iconOnly && lead) {
        float g = std::min(t.iconSize, side);
        place("lead", *lead, { tab.x + (tab.width - g) / 2, tab.y + (tab.height - g) / 2, g, g });
        return;
    }
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

    std::vector<const Activity*> started;
    for (const Activity& a : s.activities) started.push_back(&a);
    std::sort(started.begin(), started.end(), [](const Activity* a, const Activity* b) { return a->startOrder < b->startOrder; });
    // Along the bottom, under the card's content (decision 009).
    float p = t.expandedPadding, y = cardSize.y - p - t.tabHeight;
    TabStrip strip;
    strip.radius = std::min(nestedRadius, t.tabHeight / 2);
    // What's shown: Home, an activity's card, or (a dormant panel's card) neither,
    // so Home's button there is a plain way back, not the current tab.
    std::optional<std::string> shown;
    if (s.homeOpen) shown = "";
    else if (findActivity(s, s.expandedId)) shown = s.expandedId;
    auto add = [&](const TabFace& face, Rect frame, Rect hitFrame) {
        if (face.id == shown) strip.active = strip.tabs.size();
        strip.tabs.push_back({ frame, hitFrame, face.id });
        placeTab(face, frame, strip.radius, measure, elements);
    };

    // Home: an icon-only button on its own at the left edge. Its click area
    // takes in the card's bottom-left corner, so it's hard to miss. The same
    // room is kept free on the right, so the activity tabs stay centred on the card.
    float reserved = 0;
    if (!s.panels.empty()) {
        Rect button{ p, y, t.homeButtonWidth, t.tabHeight };
        float top = y - t.slotGap / 2;
        add({ "", "", &kHomeIcon, Text{}, true }, button, { 0, top, p + button.width + t.slotGap / 2, cardSize.y - top });
        reserved = button.width + t.slotGap;
    }

    // Every tab at its full width, the group centred; narrower only when they don't fit.
    float available = cardSize.x - 2 * p - 2 * reserved, n = static_cast<float>(started.size());
    float width = std::min(t.tabMaxWidth, (available - (n - 1) * t.tabGap) / n);
    float left = (cardSize.x - (n * width + (n - 1) * t.tabGap)) / 2;
    for (std::size_t i = 0; i < started.size() && n > 0; ++i)
    {
        Rect frame{ left + static_cast<float>(i) * (width + t.tabGap), y, width, t.tabHeight };
        add(faceOf(*started[i]), frame, frame);
    }
    return strip;
}

const Tab* tabAt(const TabStrip& strip, Vec2 point)
{
    for (const Tab& tab : strip.tabs)
        if (tab.hitFrame.contains(point)) return &tab;
    return nullptr;
}

} // namespace cup
