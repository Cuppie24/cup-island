// Home (decision 009): the tile grid, the Home tab, the idle clock, and the
// countdown label panels use.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tabs.h"
#include "core/tokens.h"
#include "expect.h"

#include <algorithm>
#include <cmath>

namespace cup::test {
namespace {

const Placed* find(const Scene& scene, const std::string& key)
{
    for (const auto& p : scene.elements)
        if (p.key == key) return &p;
    return nullptr;
}

// What the idle clock says at `now`, or "none".
std::string clockAt(const IslandState& s, Ms now)
{
    Scene scene = layout(s, now);
    const Placed* clock = find(scene, "clock");
    return clock ? std::get<Text>(clock->item).value : "none";
}

Panel panel(const std::string& id, bool wide = false)
{
    Content content{ .label = id, .expandedCenter = { row({ Text{ id } }) },
                     .tile = { Row{ .items = { Text{ id } }, .trailing = { Button{ Glyph::Play, "start" } } } } };
    return { .id = id, .content = content, .wide = wide, .providerId = "test" };
}

IslandState withPanels(std::initializer_list<Panel> panels)
{
    IslandState s;
    for (const Panel& p : panels) s = reduce(s, PanelPublished{ p }, Ms{ 0 });
    return s;
}

} // namespace

void runHomeTests()
{
    const Tokens& t = tokens();

    // Rows in publish order, each across the full width (decision 019): a wide
    // tile twice a narrow one, a row that isn't full stretches, centred vertically.
    IslandState s = withPanels({ panel("a"), panel("b", true), panel("c") });
    Scene home = layout(reduce(s, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    // Home always has its strip: Home's button and the pin (decision 046); the row's room (decision 109).
    float stripSpace = t.tabRowInset + t.tabHeight + t.tabRowGap - t.expandedPadding;
    expect(home.mode == Mode::Expanded && home.size.x == t.expandedSize.x && home.size.y == t.expandedSize.y + stripSpace,
           "one row: Home keeps Expanded's card size, plus its strip");
    expect(home.tabs && home.tabs->tabs.empty() && home.tabs->home && home.tabs->pin && !home.tabs->previous,
           "only Home: its button and the pin; no dots, no arrows (decision 115)");
    expect(home.label == "Home" && home.activityId.empty(), "Home is no activity's card");
    float p = t.expandedPadding, gap = t.tileGap, unit = (home.size.x - 2 * p - 2 * gap) / 4;
    expect(home.tiles.size() == 3, "a b(wide) c: one full row");
    const Tile& a = home.tiles[0];
    const Tile& b = home.tiles[1];
    const Tile& c = home.tiles[2];
    expect(a.id == "a" && a.frame.x == p && std::abs(a.frame.width - unit) < 0.01f, "first: one share, one padding in");
    expect(std::abs(b.frame.x - (a.frame.x + a.frame.width + gap)) < 0.01f && std::abs(b.frame.width - 2 * unit) < 0.01f, "wide: two shares");
    expect(c.frame.y == a.frame.y && std::abs(c.frame.x + c.frame.width - (home.size.x - p)) < 0.01f, "the row reaches the far padding");
    expect(std::abs(a.frame.y - stripSpace - p - (home.size.y - p - a.frame.y - a.frame.height)) < 0.01f, "centred vertically, under the strip");

    auto reachesEdge = [&](const Scene& scene, const Tile& last) { return std::abs(last.frame.x + last.frame.width - (scene.size.x - p)) < 0.01f; };
    IslandState pair = withPanels({ panel("m", true), panel("f", true), panel("x"), panel("y") });
    Scene two = layout(reduce(pair, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(two.tiles.size() == 4 && two.tiles[2].frame.y > two.tiles[0].frame.y && two.tiles[2].frame.x == p, "a second row");
    expect(std::abs(two.tiles[0].frame.width - two.tiles[1].frame.width) < 0.01f && reachesEdge(two, two.tiles[1]), "two wide: halves");
    expect(std::abs(two.tiles[2].frame.width - two.tiles[3].frame.width) < 0.01f && reachesEdge(two, two.tiles[3]),
           "a row that isn't full stretches to the full width");
    Scene alone = layout(reduce(withPanels({ panel("x") }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(alone.tiles.size() == 1 && std::abs(alone.tiles[0].frame.width - (alone.size.x - 2 * p)) < 0.01f, "one widget: the full width");

    IslandState wideFirst = withPanels({ panel("x"), panel("y"), panel("z"), panel("w", true) });
    Scene wrapped = layout(reduce(wideFirst, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(wrapped.tiles.size() == 4 && wrapped.tiles[3].frame.x == p && wrapped.tiles[3].frame.y > wrapped.tiles[0].frame.y &&
           reachesEdge(wrapped, wrapped.tiles[2]) && reachesEdge(wrapped, wrapped.tiles[3]),
           "a wide tile that doesn't fit starts the next row; both rows full width");

    // Nothing is hidden: Home grows to fit every row, up to the canvas.
    IslandState many;
    for (int i = 0; i < 16; ++i) many = reduce(many, PanelPublished{ panel("p" + std::to_string(i)) }, Ms{ 0 });
    Scene full = layout(reduce(many, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(full.tiles.size() == 16, "every widget shows");
    expect(full.size.y == 4 * (t.tileHeight + gap) - gap + 2 * p + stripSpace && full.size.y <= t.canvas.y, "the card grows to fit four rows");
    for (const Tile& tile : full.tiles)
        expect(tile.frame.y + tile.frame.height <= full.size.y - p + 0.01f, "tile inside the card: " + tile.id);

    // Tile content: keyed by panel, inside the tile's padding. A widget is
    // information only (decision 019): its button is left out, a click anywhere opens the card.
    const Placed* label = find(home, "tile/a/0/0");
    expect(label && label->frame.x == a.frame.x + t.tilePadding, "tile content keeps the tile's padding");
    expect(!find(home, "tile/a/0/1"), "a widget's button is left out");
    IslandState iconic = reduce(IslandState{}, PanelPublished{ { .id = "i", .content = { .tile = { row({ Icon{ Glyph::Sun }, Text{ "Light" } }) } } } }, Ms{ 0 });
    Scene iconHome = layout(reduce(iconic, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Placed* widgetIcon = find(iconHome, "tile/i/0/0");
    expect(widgetIcon && std::get<Icon>(widgetIcon->item).quiet, "a widget's icon is quiet, like a secondary button (decision 073)");
    expect(!std::get<Icon>(widgetIcon->item).large, "an icon in a widget's rows stays text-sized (decision 111)");

    // A widget's sizes (decisions 111, 114): a TileTitle line and a Body line fit a tile, its leading icon large.
    IslandState scaled = reduce(IslandState{}, PanelPublished{ { .id = "s", .content = {
        .tileLeading = { row({ Icon{ Glyph::Timer } }, Align::Center) },
        .tile = { row({ Text{ "25:00", TextStyle::TileTitle } }), row({ Text{ "Focus · 4 rounds", TextStyle::Body } }) } } } }, Ms{ 0 });
    Scene scaledHome = layout(reduce(scaled, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Placed* leadIcon = find(scaledHome, "tile/s/lead/0/0");
    expect(leadIcon && std::get<Icon>(leadIcon->item).large && leadIcon->frame.height == t.tileIconSize, "the leading icon: tileIconSize");
    const Tile& st = scaledHome.tiles[0];
    for (const char* key : { "tile/s/0/0", "tile/s/1/0" }) {
        const Placed* line = find(scaledHome, key);
        expect(line && line->frame.y >= st.frame.y + t.tilePadding - 0.01f &&
                   line->frame.y + line->frame.height <= st.frame.y + st.frame.height - t.tilePadding + 0.01f,
               std::string("a TileTitle and a Body line fit inside the tile's padding: ") + key);
    }
    expect(!hitTest(home, { a.frame.x + a.frame.width - t.tilePadding - 2, a.frame.y + a.frame.height / 2 }) && a.opens,
           "nothing on it takes the click: the tile opens its card");
    expect(tileAt(home, { a.frame.x + 2, a.frame.y + 2 }) && tileAt(home, { a.frame.x + 2, a.frame.y + 2 })->id == "a",
           "a point finds its tile");
    expect(!tileAt(home, { 1, 1 }), "outside the tiles: none");
    expect(home.tileRadius == std::min(pillCornerRadius(home.size) - p, t.tileHeight / 2), "tile corners nested in the card's");

    // Live: the tile shows the activity's tile slot, in the same place.
    Content live{ .label = "a live", .tile = { row({ Text{ "running" } }) } };
    IslandState running = reduce(s, ActivityStarted{ { .id = "a", .content = live } }, Ms{ 0 });
    expect(layout(running, Ms{ 0 }).mode == Mode::Compact, "a started panel is a normal activity in the pill");
    Scene liveScene = layout(reduce(running, HomeOpened{}, Ms{ 0 }), Ms{ 0 });
    const Placed* runningLabel = find(liveScene, "tile/a/0/0");
    expect(runningLabel && std::get<Text>(runningLabel->item).value == "running", "running: the activity's tile");
    expect(liveScene.tiles[0].frame.x == a.frame.x, "the tile stays where it was");

    // The page row (decision 109): the activities' dots, centred with ‹ and ›
    // beside them; Home has no dot, only its button apart at the top left (decision 115).
    IslandState tabbed = reduce(running, ActivityStarted{ { .id = "media", .content = { .label = "Song" } } }, Ms{ 0 });
    tabbed = reduce(tabbed, HomeOpened{}, Ms{ 0 });
    Scene strip = layout(tabbed, Ms{ 0 });
    expect(strip.tabs && strip.tabs->tabs.size() == 2 && strip.tabs->tabs[0].id == "a" && !strip.tabs->active,
           "a dot per activity, none of them Home's: on Home, none is shown");
    expect(strip.tabs->previous && strip.tabs->next, "the arrows: from Home to the activities");
    expect(strip.tabs->home && strip.tabs->home->id == kHomeTab, "and Home's button");
    const Tab& homeButton = *strip.tabs->home;
    expect(homeButton.frame == Rect{ p, t.tabRowInset, t.homeButtonWidth, t.tabHeight }, "Home: a button at the top left (decisions 047, 109)");
    expect(tabAt(*strip.tabs, { 1, 1 }) == &homeButton, "its click area takes in the card's corner");
    expect(tabAt(*strip.tabs, { p + t.homeButtonWidth + t.slotGap / 2 - 1, homeButton.frame.y + t.tabHeight + t.tabRowGap / 2 - 1 }) == &homeButton,
           "and half the gaps around it");
    expect(!tabAt(*strip.tabs, { p + t.homeButtonWidth + t.slotGap / 2 + 1, homeButton.frame.y + 2 }), "but no further");
    const Tab& before = *strip.tabs->previous;
    const Tab& after = *strip.tabs->next;
    expect(before.frame.x >= homeButton.frame.x + homeButton.frame.width + t.slotGap, "set apart from the dots and arrows");
    expect(std::abs(before.frame.x - (strip.size.x - after.frame.x - after.frame.width)) < 0.01f, "the dots and arrows stay centred on the card");
    expect(before.frame.y + before.frame.height / 2 == homeButton.frame.y + homeButton.frame.height / 2, "one row");
    float highest = strip.size.y;
    for (const Tile& tile : strip.tiles) highest = std::min(highest, tile.frame.y);
    expect(highest >= homeButton.frame.y + t.tabHeight + t.tabRowGap, "tiles sit under the strip");
    const Placed* homeIcon = find(strip, "tab/home/lead");
    expect(homeIcon && std::get<Icon>(homeIcon->item).glyph == Glyph::Home && !find(strip, "tab/home/title"), "Home: icon only");
    expect(std::abs(homeIcon->frame.x + homeIcon->frame.width / 2 - (homeButton.frame.x + t.homeButtonWidth / 2)) < 0.01f, "icon centred");
    Scene card = layout(reduce(tabbed, TileClicked{ "b" }, Ms{ 0 }), Ms{ 0 });
    expect(card.activityId == "b" && !card.tabs->active && card.tiles.empty(),
           "a dormant card: nothing selected, so Home's button is a way back");
    Scene media = layout(reduce(tabbed, ActivitySwitched{ "media" }, Ms{ 0 }), Ms{ 0 });
    expect(media.tabs->active && media.tabs->tabs[*media.tabs->active].id == "media" && media.tiles.empty(),
           "an activity's tab shows its card");
    expect(!homeIcon->muted && find(card, "tab/home/lead")->muted && find(media, "tab/home/lead")->muted,
           "Home's button in colour only on Home: grey on a card, a dormant one's too (decision 072)");
    Scene single = layout(reduce(reduce(s, ActivityStarted{ { .id = "solo" } }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(single.tabs && single.tabs->tabs.size() == 1 && single.tabs->home, "one activity plus panels: its dot and Home's button");
    Scene bare = layout(reduce(s, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(bare.tabs && bare.tabs->tabs.empty() && bare.tabs->home && bare.tabs->pin, "Home with nothing running: Home's button and the pin");
    Scene dormant = layout(reduce(reduce(s, PillClicked{}, Ms{ 0 }), TileClicked{ "a" }, Ms{ 0 }), Ms{ 0 });
    expect(dormant.tabs && dormant.tabs->tabs.empty() && dormant.tabs->home && !dormant.tabs->previous,
           "a dormant card with nothing running: just Home's button, the way back");

    // A panel's page-row button (decision 113): Settings' gear right of the pin, in the corner; not a widget.
    Panel gear{ .id = "gear", .content = { .label = "Settings", .badge = true }, .providerId = "sys", .tapAction = "open", .rowButton = Glyph::Settings };
    Scene geared = layout(reduce(reduce(s, PanelPublished{ gear }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(geared.tabs && geared.tabs->buttons.size() == 1 && geared.tabs->pin, "the gear: a button in the page row");
    const Tab& gearTab = geared.tabs->buttons[0];
    const Tab& pinTab = *geared.tabs->pin;
    expect(gearTab.id == std::string(kPanelButtonTab) + "gear" && std::abs(gearTab.frame.x + gearTab.frame.width - (geared.size.x - p)) < 0.01f &&
               gearTab.hitFrame.x + gearTab.hitFrame.width == geared.size.x,
           "the gear in the right corner, its click area to the card's edge");
    expect(pinTab.frame.x + pinTab.frame.width + t.tabArrowGap <= gearTab.frame.x + 0.01f && pinTab.hitFrame.x + pinTab.hitFrame.width <= gearTab.hitFrame.x + 0.01f,
           "the pin just left of it, their click areas apart");
    expect(std::none_of(geared.tiles.begin(), geared.tiles.end(), [](const Tile& tile) { return tile.id == "gear"; }) && geared.tiles.size() == bare.tiles.size(),
           "not a Home widget");
    const Tab* under = tabAt(*geared.tabs, { gearTab.frame.x + 2, gearTab.frame.y + 2 });
    expect(under && under->id == gearTab.id && gearTab.badge, "a point finds it; its panel's dot on it");
    const Placed* gearIcon = find(geared, "tab/button/gear/lead");
    expect(gearIcon && std::get<Icon>(gearIcon->item).glyph == Glyph::Settings && gearIcon->muted, "the gear's glyph, grey until hovered");

    // Idle clock: the local time, centred, re-laid out at the next minute.
    IslandState idle;
    expect(layout(idle, Ms{ 0 }).elements.empty() && !sceneDeadline(layout(idle, Ms{ 0 })), "no clock before the platform syncs it");
    Ms at{ 5000 };
    idle = reduce(idle, ClockSynced{ Ms{ (14 * 60 + 5) * 60000 + 30000 }, true }, at); // 14:05:30
    Scene clock = layout(idle, at);
    const Placed* time = find(clock, "clock");
    expect(time && std::get<Text>(time->item).value == "14:05", "24-hour clock");
    expect(time && std::abs(time->frame.x + time->frame.width / 2 - clock.size.x / 2) < 0.01f, "clock centred in the pill");
    expect(sceneDeadline(clock) == at + Ms{ 30000 }, "next layout at the minute");
    expect(clockAt(idle, at + Ms{ 30000 }) == "14:06", "and then it shows the new minute");
    IslandState twelve = reduce(IslandState{}, ClockSynced{ Ms{ (14 * 60 + 5) * 60000 }, false }, Ms{ 0 });
    expect(clockAt(twelve, Ms{ 0 }) == "2:05", "12-hour clock, no AM/PM");
    IslandState midnight = reduce(IslandState{}, ClockSynced{ Ms{ 59000 }, false }, Ms{ 0 });
    expect(clockAt(midnight, Ms{ 0 }) == "12:00", "12-hour midnight is 12");
    expect(clockAt(midnight, Ms{ 24 * 3600000 }) == "12:00", "wraps after a day");
    IslandState busy = reduce(idle, ActivityStarted{ { .id = "media" } }, at);
    expect(clockAt(busy, at) == "none" && !layout(busy, at).clockTick, "only the idle pill shows the clock");

    // Home has no clock tile (decision 019): the idle pill already shows the time.
    IslandState synced = reduce(withPanels({ panel("a"), Panel{ .id = "bt", .content = { .tile = { row({ Text{ "82%" } }) } } } }),
                                ClockSynced{ Ms{ (9 * 60 + 41) * 60000 + 15000 }, true }, Ms{ 0 });
    Scene widgets = layout(reduce(synced, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(widgets.tiles.size() == 2 && widgets.tiles[0].id == "a" && widgets.tiles[0].opens, "panels only; one with a card opens");
    expect(widgets.tiles[1].id == "bt" && !widgets.tiles[1].opens, "a static widget doesn't");
    expect(!sceneDeadline(widgets), "nothing ticks on Home without a clock");

    // The leading square: as tall as the tile's inside, at its left; the rows beside it.
    Panel art{ .id = "m", .content = { .tileLeading = { row({ Image{} }) },
                                       .tile = { row({ Text{ "Song" } }), Row{ .items = { Text{ "Band" } }, .trailing = { Button{ Glyph::Play, "p" } } } } },
               .wide = true };
    Scene leading = layout(reduce(withPanels({ art }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Tile& m = leading.tiles[0];
    const Placed* cover = find(leading, "tile/m/lead/0/0");
    const Placed* song = find(leading, "tile/m/0/0");
    float inside = m.frame.height - 2 * t.tilePadding;
    expect(cover && cover->frame == Rect{ m.frame.x + t.tilePadding, m.frame.y + t.tilePadding, inside, inside }, "art fills the square");
    expect(song && song->frame.x == cover->frame.x + inside + t.slotGap, "the rows start past it");
    expect(find(leading, "tile/m/1/0") && !find(leading, "tile/m/1/1"), "the row keeps its text, loses its button");

    // A tap-action tile (a toggle): clickable, sends its action, opens no card even with one.
    Panel toggle{ .id = "theme", .content = { .expandedCenter = { row({ Text{ "card" } }) }, .tile = { row({ Text{ "Dark" } }) } },
                  .providerId = "sys", .tapAction = "toggle" };
    IslandState toggled = reduce(withPanels({ toggle }), PillClicked{}, Ms{ 0 });
    Scene toggleHome = layout(toggled, Ms{ 0 });
    expect(toggleHome.tiles.size() == 1 && !toggleHome.tiles[0].opens && toggleHome.tiles[0].clickable() &&
           toggleHome.tiles[0].tapAction == "toggle" && toggleHome.tiles[0].providerId == "sys",
           "a toggle tile carries its action and provider, and lights up");
    expect(!widgets.tiles[1].clickable(), "static widgets don't");

    // Countdown labels: time left without the "-".
    Timeline timer{ .duration = Ms{ 1500000 }, .anchoredAt = Ms{ 0 }, .playing = true };
    IslandState counting;
    counting = reduce(counting, ActivityStarted{ { .id = "t", .content = { .expandedBottom = { row({ TimeLabel{ .timeline = timer, .countdown = true } }) } } } }, Ms{ 0 });
    counting = reduce(counting, PillClicked{}, Ms{ 0 });
    Scene countdown = layout(counting, Ms{ 1 });
    const Placed* left = find(countdown, "expandedBottom/0/0");
    expect(left && std::get<Text>(left->item).value == "24:59" && std::get<Text>(left->item).centred, "countdown: 24:59, no minus, centred");

    // "New" (decision 110): drawn on an announced widget's first line until its card is opened.
    Panel fresh = panel("fresh");
    fresh.announce = 1;
    Panel still = panel("still"); // no card to open: never New
    still.announce = 1;
    still.content.expandedCenter.clear();
    IslandState news = withPanels({ fresh, still });
    auto tagOn = [](const Scene& scene, const std::string& tile) {
        for (const Placed& p : scene.elements)
            if (auto* tag = std::get_if<Tag>(&p.item); tag && tag->text == "New" && p.key.rfind("tile/" + tile + "/", 0) == 0) return &p;
        return static_cast<const Placed*>(nullptr);
    };
    Scene unseen = layout(reduce(news, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Placed* tag = tagOn(unseen, "fresh");
    const Placed* name = find(unseen, "tile/fresh/0/0");
    expect(tag && name && tag->frame.x > name->frame.x && std::abs(tag->frame.y + tag->frame.height / 2 - (name->frame.y + name->frame.height / 2)) < 1,
           "an announced widget: New on its first line, after its text");
    expect(!tagOn(unseen, "still"), "announced but with no card: nothing to open, so never New");
    news = reduce(reduce(news, PillClicked{}, Ms{ 0 }), TileClicked{ "fresh" }, Ms{ 0 });
    news = reduce(reduce(news, EscapePressed{}, Ms{ 0 }), HomeOpened{}, Ms{ 0 });
    expect(!tagOn(layout(news, Ms{ 0 }), "fresh"), "its card opened once: no more New");
    expect(news.seenPanels == std::map<std::string, int>{ { "fresh", 1 } }, "and the host remembers it (the app saves seenPanels)");
    // The provider announces it again (a feature added to it): New again, until opened again.
    fresh.announce = 2;
    news = reduce(news, PanelPublished{ fresh }, Ms{ 0 });
    expect(tagOn(layout(news, Ms{ 0 }), "fresh") != nullptr, "announced again: New again");
    news = reduce(reduce(news, TileClicked{ "fresh" }, Ms{ 0 }), HomeOpened{}, Ms{ 0 });
    expect(!tagOn(layout(news, Ms{ 0 }), "fresh") && news.seenPanels["fresh"] == 2, "...until its card is opened again");
}

} // namespace cup::test
