// Home (decision 009): the tile grid, the Home tab, the idle clock, and the
// countdown label panels use.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"

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

    // The grid: publish order, one column or two, centred vertically in Expanded's card.
    IslandState s = withPanels({ panel("a"), panel("b", true), panel("c"), panel("d", true), panel("e") });
    Scene home = layout(reduce(s, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(home.mode == Mode::Expanded && home.size == t.expandedSize, "Home keeps Expanded's card size");
    expect(!home.tabs, "no tab strip with only Home");
    expect(home.label == "Home" && home.activityId.empty(), "Home is no activity's card");
    float p = t.expandedPadding, gap = t.tileGap, column = (home.size.x - 2 * p - 3 * gap) / 4;
    expect(home.tiles.size() == 5, "two rows: a b(wide) c / d(wide) e");
    const Tile& a = home.tiles[0];
    const Tile& b = home.tiles[1];
    const Tile& c = home.tiles[2];
    const Tile& d = home.tiles[3];
    expect(a.id == "a" && a.frame.x == p && std::abs(a.frame.width - column) < 0.01f, "first tile: one column, one padding in");
    expect(b.frame.x == a.frame.x + a.frame.width + gap && std::abs(b.frame.width - (2 * column + gap)) < 0.01f, "wide tile: two columns");
    expect(c.frame.y == a.frame.y && std::abs(c.frame.x + c.frame.width - (home.size.x - p)) < 0.01f, "row fills to the far padding");
    expect(d.frame.x == p && d.frame.y == a.frame.y + t.tileHeight + gap, "next row");
    float top = a.frame.y, bottom = d.frame.y + d.frame.height;
    expect(std::abs(top - p - (home.size.y - p - bottom)) < 0.01f, "grid centred vertically");

    IslandState wideFirst = withPanels({ panel("x"), panel("y"), panel("z"), panel("w", true) });
    Scene wrapped = layout(reduce(wideFirst, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(wrapped.tiles.size() == 4 && wrapped.tiles[3].frame.x == p && wrapped.tiles[3].frame.y > wrapped.tiles[0].frame.y,
           "a wide tile that doesn't fit the row's rest starts the next row");

    IslandState many;
    for (int i = 0; i < 12; ++i) many = reduce(many, PanelPublished{ panel("p" + std::to_string(i)) }, Ms{ 0 });
    Scene full = layout(reduce(many, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(full.tiles.size() == 8, "tiles that don't fit the card are left out");
    for (const Tile& tile : full.tiles)
        expect(tile.frame.y + tile.frame.height <= full.size.y - p, "tile inside the card: " + tile.id);

    // Tile content: keyed by panel, inside the tile's padding, its button routes to the provider.
    const Placed* label = find(home, "tile/a/0/0");
    const Placed* start = find(home, "tile/a/0/1");
    expect(label && label->frame.x == a.frame.x + t.tilePadding, "tile content keeps the tile's padding");
    expect(start && start->frame.x + start->frame.width == a.frame.x + a.frame.width - t.tilePadding, "trailing button at the tile's edge");
    auto hit = hitTest(home, { start->frame.x + start->frame.width / 2, start->frame.y + start->frame.height / 2 });
    expect(hit && hit->action == Action{ "test", "a", "start", 0 }, "a tile's button routes to its provider, with the panel's id");
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

    // Tabs along the bottom: Home's square icon button apart at the left, then
    // activities, centred on the card; Home stays selected for a dormant panel's card.
    IslandState tabbed = reduce(running, ActivityStarted{ { .id = "media", .content = { .label = "Song" } } }, Ms{ 0 });
    tabbed = reduce(tabbed, HomeOpened{}, Ms{ 0 });
    Scene strip = layout(tabbed, Ms{ 0 });
    expect(strip.tabs && strip.tabs->tabs.size() == 3 && strip.tabs->tabs[0].id.empty() && strip.tabs->active == 0,
           "Home button first and selected on Home");
    const Tab& homeButton = strip.tabs->tabs[0];
    expect(homeButton.frame == Rect{ p, strip.size.y - p - t.tabHeight, t.tabHeight, t.tabHeight }, "Home: a square at the bottom left");
    const Tab& first = strip.tabs->tabs[1];
    const Tab& last = strip.tabs->tabs[2];
    expect(first.frame.x >= homeButton.frame.x + homeButton.frame.width + t.slotGap, "set apart from the activity tabs");
    expect(std::abs(first.frame.x - (strip.size.x - last.frame.x - last.frame.width)) < 0.01f, "activity tabs stay centred on the card");
    expect(first.frame.y == homeButton.frame.y, "one strip");
    float lowest = 0;
    for (const Tile& tile : strip.tiles) lowest = std::max(lowest, tile.frame.y + tile.frame.height);
    expect(lowest + t.slotGap <= homeButton.frame.y, "tiles sit above the strip");
    const Placed* homeIcon = find(strip, "tab/home/lead");
    expect(homeIcon && std::get<Icon>(homeIcon->item).glyph == Glyph::Home && !find(strip, "tab/home/title"), "Home: icon only");
    expect(std::abs(homeIcon->frame.x + homeIcon->frame.width / 2 - (homeButton.frame.x + t.tabHeight / 2)) < 0.01f, "icon centred");
    Scene card = layout(reduce(tabbed, TileClicked{ "b" }, Ms{ 0 }), Ms{ 0 });
    expect(card.activityId == "b" && card.tabs->active == 0 && card.tiles.empty(), "a dormant card: Home tab stays selected");
    Scene media = layout(reduce(tabbed, ActivitySwitched{ "media" }, Ms{ 0 }), Ms{ 0 });
    expect(media.tabs->tabs[media.tabs->active].id == "media" && media.tiles.empty(), "an activity's tab shows its card");
    Scene single = layout(reduce(reduce(s, ActivityStarted{ { .id = "solo" } }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(single.tabs && single.tabs->tabs.size() == 2, "one activity plus panels: Home and its tab");
    expect(!layout(reduce(s, PillClicked{}, Ms{ 0 }), Ms{ 0 }).tabs, "Home with nothing running: no strip");
    Scene dormant = layout(reduce(reduce(s, PillClicked{}, Ms{ 0 }), TileClicked{ "a" }, Ms{ 0 }), Ms{ 0 });
    expect(dormant.tabs && dormant.tabs->tabs.size() == 1 && dormant.tabs->tabs[0].id.empty(),
           "a dormant card with nothing running: just the way back Home");

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

    // Home's clock: the first tile, built from the synced clock; a static widget, no card.
    IslandState dated = reduce(withPanels({ panel("a"), Panel{ .id = "battery", .content = { .tile = { row({ Text{ "82%" } }) } } } }),
                               ClockSynced{ Ms{ (9 * 60 + 41) * 60000 + 15000 }, true, "Sat 11 Oct" }, Ms{ 0 });
    Scene withClock = layout(reduce(dated, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(withClock.tiles.size() == 3 && withClock.tiles[0].id.empty() && !withClock.tiles[0].opens, "clock first, not clickable");
    expect(withClock.tiles[1].id == "a" && withClock.tiles[1].opens, "a panel with a card opens");
    expect(withClock.tiles[2].id == "battery" && !withClock.tiles[2].opens, "a static widget doesn't");
    const Placed* bigTime = find(withClock, "tile/clock/0/0");
    const Placed* date = find(withClock, "tile/clock/1/0");
    expect(bigTime && std::get<Text>(bigTime->item).value == "9:41" && std::get<Text>(bigTime->item).style == TextStyle::Title,
           "clock tile: the time, large");
    expect(date && std::get<Text>(date->item).value == "Sat 11 Oct", "and the date under it");
    expect(sceneDeadline(withClock) == Ms{ 45000 }, "Home re-lays out at the minute while the clock shows");

    // Countdown labels: time left without the "-".
    Timeline timer{ .duration = Ms{ 1500000 }, .anchoredAt = Ms{ 0 }, .playing = true };
    IslandState counting;
    counting = reduce(counting, ActivityStarted{ { .id = "t", .content = { .expandedBottom = { row({ TimeLabel{ .timeline = timer, .countdown = true } }) } } } }, Ms{ 0 });
    counting = reduce(counting, PillClicked{}, Ms{ 0 });
    Scene countdown = layout(counting, Ms{ 1 });
    const Placed* left = find(countdown, "expandedBottom/0/0");
    expect(left && std::get<Text>(left->item).value == "24:59" && std::get<Text>(left->item).alignEnd, "countdown: 24:59, no minus");
}

} // namespace cup::test
