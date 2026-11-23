#include "core/tabs.h"

#include "core/tokens.h"

#include <algorithm>

namespace cup {
namespace {

// One icon of the list: a panel's card, live or dormant, or a panel-less activity.
struct Page {
    std::string id;
    Glyph glyph = Glyph::Source;
    bool badge = false, accent = false;
};

// The list (decision 165): panels with a card in publish order, whether
// running or not, then activities that aren't panels in start order. It only
// changes when a provider comes or goes, so an icon keeps its place.
std::vector<Page> pagesOf(const IslandState& s)
{
    std::vector<Page> pages;
    for (const Panel& panel : s.panels) {
        if (panel.rowButton) continue; // a button of its own, at one of the row's ends
        const Activity* live = findActivity(s, panel.id);
        if (!live && !isPage(panel)) continue; // a toggle, or nothing to show: no card
        const Content& shows = live ? live->content : panel.content;
        pages.push_back({ panel.id, panel.icon.value_or(Glyph::Source), shows.badge || shows.badgeAccent, shows.badgeAccent });
    }
    std::vector<const Activity*> others;
    for (const Activity& a : s.activities)
        if (!findPanel(s, a.id)) others.push_back(&a);
    std::sort(others.begin(), others.end(), [](const Activity* a, const Activity* b) { return a->startOrder < b->startOrder; });
    for (const Activity* a : others) pages.push_back({ a->id, Glyph::Source, a->content.badge || a->content.badgeAccent, a->content.badgeAccent });
    return pages;
}

// Somewhere else to go: another page, or a panel's button (Settings, Search).
bool hasTabs(const IslandState& s)
{
    if (mode(s) != Mode::Expanded) return false;
    if (auto card = shownCard(s)) { // a card that hides them (Content::hidesTabs)
        const Content* c = cardContent(s, card->activityId);
        if (c && c->hidesTabs) return false;
    }
    bool buttons = std::any_of(s.panels.begin(), s.panels.end(), [](const Panel& p) { return p.rowButton.has_value(); });
    return buttons || pagesOf(s).size() > 1;
}

// A button's glyph, centred in its square. Keys carry what it is, so it stays
// the same visual as the row changes. `muted`: grey until the pointer is on
// that button itself (decisions 072, 165): its activityId is the button's id,
// which only its own hover lights.
void placeGlyph(const std::string& name, const std::string& id, Glyph glyph, Rect button, bool muted, std::vector<Placed>& out)
{
    float g = std::min(tokens().iconSize, button.height);
    Rect frame{ button.x + (button.width - g) / 2, button.y + (button.height - g) / 2, g, g };
    out.push_back({ .key = "tab/" + name + "/lead", .item = Icon{ .glyph = glyph }, .frame = frame, .hitFrame = frame, .activityId = id, .muted = muted });
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

    // Along the top, over the card's content (decision 047), or the bottom, under it: the user's pick (decision 048).
    float p = t.expandedPadding, h = t.tabHeight, y = s.tabsOnTop ? t.tabRowInset : cardSize.y - t.tabRowInset - h;
    TabStrip strip;
    strip.radius = std::min(nestedRadius, h / 2);
    // Click areas reach the card's edge on the row's side and half the gap on the content's.
    float top = s.tabsOnTop ? 0 : y - t.tabRowGap / 2, bottom = s.tabsOnTop ? y + h + t.tabRowGap / 2 : cardSize.y;
    auto button = [&](const std::string& id, Rect frame, float reachLeft, float reachRight) {
        return Tab{ frame, { frame.x - reachLeft, top, frame.width + reachLeft + reachRight, bottom - top }, id, false, frame, strip.radius };
    };
    float w = t.rowButtonWidth, between = t.tabIconGap;
    float left = p, right = cardSize.x - p; // what the edges' buttons leave for the list

    // Panels' buttons (decisions 113, 165), grey until hovered: the corner's
    // from the right edge (Settings), then those beside them (the theme
    // switch); the left corner's from the left edge (Search). A corner's
    // click area takes in the card's corner.
    auto panelButton = [&](const Panel& panel, Rect frame, float reachLeft, float reachRight) {
        const Activity* live = findActivity(s, panel.id);
        std::string id = kPanelButtonTab + panel.id;
        strip.buttons.push_back(button(id, frame, reachLeft, reachRight));
        const Content& shows = live ? live->content : panel.content;
        strip.buttons.back().badge = shows.badge || shows.badgeAccent;
        strip.buttons.back().accent = shows.badgeAccent; // an update waiting, behind Settings' gear (decision 152)
        placeGlyph("button/" + panel.id, id, *panel.rowButton, frame, true, elements);
    };
    for (Panel::RowSpot spot : { Panel::RowSpot::Corner, Panel::RowSpot::BesideCorner })
        for (const Panel& panel : s.panels) {
            if (!panel.rowButton || panel.rowSpot != spot) continue;
            bool corner = right == cardSize.x - p;
            panelButton(panel, { right - w, y, w, h }, between / 2, corner ? p : between / 2);
            right -= w + between;
        }
    for (const Panel& panel : s.panels) {
        if (!panel.rowButton || panel.rowSpot != Panel::RowSpot::LeftCorner) continue;
        bool corner = left == p;
        panelButton(panel, { left, y, w, h }, corner ? p : between / 2, between / 2);
        left += w + between;
    }

    // The pages' icons, centred on the card as one group; squeezed between
    // the edges' buttons if there are too many to fit at their own size.
    std::vector<Page> pages = pagesOf(s);
    if (pages.empty()) return strip;
    std::optional<std::string> shown;
    if (auto card = shownCard(s)) shown = card->activityId;
    float n = static_cast<float>(pages.size());
    float room = std::max(0.0f, std::min(cardSize.x / 2 - left, right - cardSize.x / 2) * 2); // centred: as far each way
    float size = std::min(t.tabIconSize, h), step = std::min(size + between, room / n);
    size = std::min(size, step);
    float x = (cardSize.x - step * n) / 2;
    for (const Page& page : pages) {
        Rect frame{ x + (step - size) / 2, y + (h - size) / 2, size, size };
        bool current = page.id == shown;
        if (current) strip.active = strip.tabs.size();
        strip.tabs.push_back({ frame, { x, top, step, bottom - top }, page.id, page.badge, frame, std::min(strip.radius, size / 2), page.accent });
        placeGlyph("page/" + page.id, page.id, page.glyph, frame, !current, elements);
        x += step;
    }
    return strip;
}

const Tab* tabAt(const TabStrip& strip, Vec2 point)
{
    for (const Tab& tab : strip.tabs)
        if (tab.hitFrame.contains(point)) return &tab;
    for (const Tab& button : strip.buttons)
        if (button.hitFrame.contains(point)) return &button;
    return nullptr;
}

} // namespace cup
