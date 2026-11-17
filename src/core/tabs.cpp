#include "core/tabs.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// Somewhere else to go: another activity, or Home (with panels, by its
// button) from any card. And Home itself, even with nothing running: the
// strip carries Home's button and panels' buttons (decisions 009, 113).
bool hasTabs(const IslandState& s)
{
    if (mode(s) != Mode::Expanded) return false;
    if (auto card = shownCard(s)) { // a card that hides them (Content::hidesTabs)
        const Activity* a = findActivity(s, card->activityId);
        const Panel* p = a ? nullptr : findPanel(s, card->activityId);
        if ((a && a->content.hidesTabs) || (p && p->content.hidesTabs)) return false;
    }
    bool home = hasWidgets(s);
    bool dormantCard = !s.homeOpen && !findActivity(s, s.expandedId);
    return s.activities.size() + (home ? 1 : 0) > 1 || (home && (dormantCard || s.homeOpen));
}

// A button's glyph, centred in its square. Keys carry what it is, so it stays
// the same visual as the strip changes. `muted`: grey until the pointer is on
// that button itself (decisions 072, 109): its activityId is the button's id,
// which only its own hover lights: Home's while Home isn't shown, a panel's button
// while off, the arrows always. `disabled`: dim, and never lit (an arrow with nowhere to go).
void placeGlyph(const std::string& name, const std::string& id, Glyph glyph, Rect button, bool muted, std::vector<Placed>& out,
                bool disabled = false)
{
    float g = std::min(tokens().iconSize, button.height);
    Rect frame{ button.x + (button.width - g) / 2, button.y + (button.height - g) / 2, g, g };
    out.push_back({ .key = "tab/" + name + "/lead", .item = Icon{ .glyph = glyph, .disabled = disabled }, .frame = frame, .hitFrame = frame,
                    .activityId = id, .muted = muted && !disabled });
}

} // namespace

float tabStripSpace(const IslandState& s)
{
    // The row sits tabRowInset from the card's edge, tabRowGap from its content,
    // which keeps its padding beyond that: the card grows by the difference (decision 109).
    const Tokens& t = tokens();
    return hasTabs(s) ? std::max(0.0f, t.tabRowInset + t.tabHeight + t.tabRowGap - t.expandedPadding) : 0;
}

std::optional<TabStrip> tabStripFor(const IslandState& s, Vec2 cardSize, float nestedRadius, const MeasureText&,
                                    std::vector<Placed>& elements)
{
    if (!hasTabs(s)) return std::nullopt;
    const Tokens& t = tokens();

    std::vector<const Activity*> started;
    for (const Activity& a : s.activities) started.push_back(&a);
    std::sort(started.begin(), started.end(), [](const Activity* a, const Activity* b) { return a->startOrder < b->startOrder; });
    // Along the top, over the card's content (decision 047), or the bottom, under it: the user's pick (decision 048).
    float p = t.expandedPadding, h = t.tabHeight, y = s.tabsOnTop ? t.tabRowInset : cardSize.y - t.tabRowInset - h;
    TabStrip strip;
    strip.radius = std::min(nestedRadius, h / 2);
    // Click areas reach the card's edge on the strip's side and half the gap on the content's.
    float top = s.tabsOnTop ? 0 : y - t.tabRowGap / 2, bottom = s.tabsOnTop ? y + h + t.tabRowGap / 2 : cardSize.y;
    auto button = [&](const std::string& id, Rect frame, float reachLeft, float reachRight) {
        return Tab{ frame, { frame.x - reachLeft, top, frame.width + reachLeft + reachRight, bottom - top }, id, false, frame, strip.radius };
    };

    // Home's button at the left edge, its click area taking in the card's
    // corner (decision 009).
    bool homeShown = s.homeOpen;
    if (hasWidgets(s)) {
        Rect frame{ p, y, t.homeButtonWidth, h };
        strip.home = button(kHomeTab, frame, p, t.slotGap / 2);
        placeGlyph("home", kHomeTab, Glyph::Home, frame, !homeShown, elements);
    }
    // Panels' buttons at the right edge, the first in the corner (decision 113): Settings.
    float right = cardSize.x - p, w = t.homeButtonWidth, between = t.tabArrowGap;
    for (const Panel& panel : s.panels) {
        if (!panel.rowButton) continue;
        const Activity* live = findActivity(s, panel.id);
        Rect frame{ right - w, y, w, h };
        std::string id = kPanelButtonTab + panel.id;
        strip.buttons.push_back(button(id, frame, between / 2, strip.buttons.empty() ? p : between / 2));
        const Content& shows = live ? live->content : panel.content;
        strip.buttons.back().badge = shows.badge || shows.badgeAccent;
        strip.buttons.back().accent = shows.badgeAccent; // an update waiting, behind Settings' gear (decision 152)
        placeGlyph("button/" + panel.id, id, *panel.rowButton, frame, true, elements); // grey until hovered, like the arrows
        right -= w + between;
    }
    // No pin any more (decision 121): the row is Home, the pages and their arrows, and panels' buttons.

    // The pages: the activities; the shown one is a line, the rest dots (decision 109).
    // Home isn't one: its button at the left is the way there (decision 115).
    std::optional<std::string> shown;
    if (!s.homeOpen && findActivity(s, s.expandedId)) shown = s.expandedId;
    std::vector<std::pair<std::string, bool>> pages; // id, badge
    for (const Activity* a : started) pages.push_back({ a->id, a->content.badge });
    float dot = t.tabDotSize, gap = t.tabDotGap;
    auto widthOf = [&](const std::string& id) { return id == shown ? std::max(t.tabLineWidth, dot) : dot; };
    float dotsWidth = 0;
    for (const auto& page : pages) dotsWidth += widthOf(page.first);
    if (!pages.empty()) dotsWidth += gap * static_cast<float>(pages.size() - 1);
    // The arrows beside the dots, the three centred as one group. Always
    // there with any page, so the row doesn't shift (decision 118); with
    // nowhere to go (one page, shown) they're dim, and no button: nothing to click, hover or focus.
    bool arrows = !pages.empty();
    bool live = pages.size() > 1 || (!pages.empty() && !shown);
    float arrow = arrows ? t.tabArrowSize : 0, arrowGap = arrows ? t.tabArrowGap : 0;
    float x = (cardSize.x - dotsWidth - 2 * (arrow + arrowGap)) / 2;
    if (arrows) {
        Rect frame{ x, y + (h - arrow) / 2, arrow, arrow };
        if (live) {
            strip.previous = button(kPreviousTab, frame, 0, arrowGap / 2);
            strip.previous->radius = arrow / 2; // round, like the dots
        }
        placeGlyph("previous", kPreviousTab, Glyph::ChevronLeft, frame, true, elements, !live); // grey until hovered
    }
    x += arrow + arrowGap;
    for (const auto& [id, badge] : pages) {
        float w = widthOf(id);
        Rect frame{ x, y + (h - dot) / 2, w, dot };
        if (id == shown) strip.active = strip.tabs.size();
        float halo = t.tabDotGap / 2; // lit: a soft round shape a little bigger than the dot or line
        Rect lit{ frame.x - halo, frame.y - halo, w + 2 * halo, dot + 2 * halo };
        strip.tabs.push_back({ frame, { x - gap / 2, top, w + gap, bottom - top }, id, badge, lit, lit.height / 2 }); // easy to hit: the row's height
        x += w + gap;
    }
    if (!pages.empty()) x += arrowGap - gap;
    if (arrows) {
        Rect frame{ x, y + (h - arrow) / 2, arrow, arrow };
        if (live) {
            strip.next = button(kNextTab, frame, arrowGap / 2, 0);
            strip.next->radius = arrow / 2;
        }
        placeGlyph("next", kNextTab, Glyph::ChevronRight, frame, true, elements, !live);
    }
    return strip;
}

const Tab* tabAt(const TabStrip& strip, Vec2 point)
{
    for (const Tab& tab : strip.tabs)
        if (tab.hitFrame.contains(point)) return &tab;
    for (const std::optional<Tab>* button : { &strip.previous, &strip.next, &strip.home })
        if (*button && (*button)->hitFrame.contains(point)) return &**button;
    for (const Tab& button : strip.buttons)
        if (button.hitFrame.contains(point)) return &button;
    return nullptr;
}

} // namespace cup
