// Notification badges (decision 038): where the dot goes on each kind of
// element, from the flags providers set.

#include "core/badges.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"

#include <algorithm>
#include <cmath>

namespace cup::test {
namespace {

bool near(Vec2 a, Vec2 b) { return std::abs(a.x - b.x) < 0.01f && std::abs(a.y - b.y) < 0.01f; }

} // namespace

void runBadgeTests()
{
    const Tokens& t = tokens();
    float r = t.badgeSize / 2;
    auto corner = [&](float radius) { return std::max(radius, r + t.badgeInset); }; // the corner arc's centre, in from both edges

    // A flagged panel: a dot on its icon in the page row (decision 165), so the way to it is marked from any card.
    IslandState pages;
    Panel plain{ .id = "plain", .content = { .label = "p", .expandedCenter = { row({ Text{ "p" } }) } } };
    Panel flagged = plain;
    flagged.id = "flagged";
    flagged.content.badge = true;
    pages = reduce(pages, PanelPublished{ plain }, Ms{ 0 });
    pages = reduce(pages, PanelPublished{ flagged }, Ms{ 0 });
    pages = reduce(pages, PillClicked{}, Ms{ 0 });
    Scene grid = layout(pages, Ms{ 0 });
    const Tab& marked = grid.tabs->tabs[1];
    expect(grid.badges.size() == 1 && marked.badge && !grid.tabs->tabs[0].badge &&
               near(grid.badges[0], { marked.frame.x + marked.frame.width - corner(marked.radius), marked.frame.y + corner(marked.radius) }),
           "badges: a flagged panel, a dot at its icon's corner's arc centre; the rest none");

    // A tab: its activity's flag, the same corner.
    IslandState tabs;
    Activity media{ .id = "media", .content = { .label = "m", .badge = true, .compactLeading = { row({ Icon{} }) },
                                                .expandedCenter = { row({ Text{ "m" } }) } } };
    tabs = reduce(tabs, ActivityStarted{ media }, Ms{ 0 });
    tabs = reduce(tabs, ActivityStarted{ { .id = "timer", .content = { .label = "t", .compactLeading = { row({ Icon{} }) } } } }, Ms{ 0 });
    tabs = reduce(tabs, PillClicked{}, Ms{ 0 });
    Scene strip = layout(tabs, Ms{ 0 });
    bool tabDot = false;
    for (const Tab& tab : strip.tabs->tabs)
        if (tab.id == "media")
            for (Vec2 dot : strip.badges)
                tabDot = tabDot || near(dot, { tab.frame.x + tab.frame.width - corner(tab.radius), tab.frame.y + corner(tab.radius) });
    expect(strip.tabs && tabDot, "badges: a flagged activity's icon gets one, in its corner like a button's (decision 165)");

    IslandState withHome = tabs;
    withHome = reduce(withHome, PanelPublished{ flagged }, Ms{ 0 });

    // A panel's page-row button (Settings' gear): an update's dot in the accent, apart from the red ones.
    Panel gear{ .id = "gear", .content = { .label = "g", .badgeAccent = true }, .tapAction = "open", .rowButton = Glyph::Settings };
    withHome = reduce(withHome, PanelPublished{ gear }, Ms{ 0 });
    Scene geared = layout(withHome, Ms{ 0 });
    const Tab& button = geared.tabs->buttons.at(0);
    expect(button.badge && button.accent && geared.accentBadges.size() == 1 && geared.accentBadges[0].x > button.frame.x &&
               geared.accentBadges[0].x < button.frame.x + button.frame.width &&
               std::none_of(geared.badges.begin(), geared.badges.end(), [&](Vec2 c) { return near(c, geared.accentBadges[0]); }),
           "badges: Content::badgeAccent puts the gear's dot with the accent ones, not the red ones");

    // A button: on its top-right corner. In a list, only while in view.
    Slot rows;
    for (int i = 0; i < 12; ++i)
        rows.push_back(Row{ .items = { Text{ "row" }, Button{ .glyph = Glyph::Download, .actionId = "a" + std::to_string(i), .compact = true,
                                                              .badge = i == 0 || i == 11 } },
                            .list = "l", .listItem = true });
    IslandState card;
    card = reduce(card, ActivityStarted{ { .id = "card", .content = { .label = "c", .card = CardLayout::Centred, .expandedCenter = rows } } }, Ms{ 0 });
    card = reduce(card, PillClicked{}, Ms{ 0 });
    Scene listed = layout(card, Ms{ 0 });
    const Placed* first = nullptr;
    for (const Placed& p : listed.elements)
        if (auto* b = std::get_if<Button>(&p.item); b && b->actionId == "a0") first = &p;
    float round = first->frame.width / 2, out = round * 0.7071f;
    Vec2 centre{ first->frame.x + round, first->frame.y + first->frame.height / 2 };
    expect(listed.badges.size() == 1 && near(listed.badges[0], { centre.x + out, centre.y - out }),
           "badges: a flagged button, a dot on its round outline at the top right; one scrolled out of its list's window, none");
}

} // namespace cup::test
