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

    // A widget on Home: a dot in its top-right corner, badgeInset in; none on the others.
    IslandState home;
    Panel plain{ .id = "plain", .content = { .label = "p", .expandedCenter = { row({ Text{ "p" } }) }, .tile = { row({ Text{ "p" } }) } } };
    Panel flagged = plain;
    flagged.id = "flagged";
    flagged.content.badge = true;
    home = reduce(home, PanelPublished{ plain }, Ms{ 0 });
    home = reduce(home, PanelPublished{ flagged }, Ms{ 0 });
    home = reduce(home, HomeOpened{}, Ms{ 0 });
    Scene grid = layout(home, Ms{ 0 });
    const Tile& tile = grid.tiles[1];
    expect(grid.badges.size() == 1 && tile.badge && !grid.tiles[0].badge &&
           near(grid.badges[0], { tile.frame.x + tile.frame.width - corner(grid.tileRadius), tile.frame.y + corner(grid.tileRadius) }),
           "badges: a flagged widget, a dot at its corner's arc centre (concentric with its rounding); the rest none");
    expect(grid.tileRadius >= r + t.badgeInset, "badges: (a widget's rounding is round enough to set the dot's place)");

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
                tabDot = tabDot || near(dot, { tab.frame.x + tab.frame.width - corner(strip.tabs->radius), tab.frame.y + corner(strip.tabs->radius) });
    expect(strip.tabs && tabDot, "badges: a flagged activity's tab gets one");

    // Home's button: only a widget flagged for it (an update), not any widget with a dot.
    auto homeDot = [&](const Scene& sc) {
        for (const Tab& tab : sc.tabs->tabs)
            if (tab.id.empty()) return tab.badge;
        return false;
    };
    IslandState withHome = tabs;
    withHome = reduce(withHome, PanelPublished{ flagged }, Ms{ 0 });
    expect(!homeDot(layout(withHome, Ms{ 0 })), "badges: a widget's dot doesn't light Home's button");
    Panel update = flagged;
    update.id = "update";
    update.content.badgeHome = true;
    withHome = reduce(withHome, PanelPublished{ update }, Ms{ 0 });
    Scene homeLit = layout(withHome, Ms{ 0 });
    expect(homeDot(homeLit) && homeLit.badges.size() >= 2, "badges: one flagged for Home (an update) lights Home's button too");

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
