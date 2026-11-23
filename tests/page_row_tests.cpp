// The page row's buttons (decisions 113, 165, 170), the idle pill's clock
// (decision 009) and what the idle pill opens with no Home (decision 170).

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

Panel panel(const std::string& id)
{
    return { .id = id, .content = { .label = id, .expandedCenter = { row({ Text{ id } }) } }, .providerId = "test" };
}

} // namespace

void runPageRowTests()
{
    const Tokens& t = tokens();
    float p = t.expandedPadding;
    IslandState s;
    for (const char* id : { "a", "b", "c" }) s = reduce(s, PanelPublished{ panel(id) }, Ms{ 0 });

    // No Home (decision 170): the idle pill opens the first page's card, at every card's size plus the row's room.
    Scene first = layout(reduce(s, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    float stripSpace = t.tabRowInset + t.tabHeight + t.tabRowGap - t.expandedPadding;
    expect(first.mode == Mode::Expanded && first.activityId == "a" && first.label == "a", "the idle pill opens the first page's card");
    expect(first.size.x == t.expandedSize.x && first.size.y == t.expandedSize.y + stripSpace, "at Expanded's card size, plus its page row");
    expect(first.tabs && first.tabs->tabs.size() == 3 && first.tabs->active == 0 && first.tabs->buttons.empty(),
           "the row: every panel's icon, the shown one lit; nothing else without panels' buttons");
    Scene lone = layout(reduce(reduce(IslandState{}, PanelPublished{ panel("only") }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(lone.activityId == "only" && !lone.tabs && lone.size.y == t.expandedSize.y, "one page and no buttons: nowhere else to go, no row");

    // A panel's page-row button (decision 113): Settings' gear in the right corner; not a page.
    Panel gear{ .id = "gear", .content = { .label = "Settings", .badge = true }, .providerId = "sys", .tapAction = "open", .rowButton = Glyph::Settings };
    Scene geared = layout(reduce(reduce(s, PanelPublished{ gear }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(geared.tabs && geared.tabs->buttons.size() == 1 && geared.tabs->tabs.size() == 3, "the gear: a button in the page row, not an icon in its list");
    const Tab& gearTab = geared.tabs->buttons[0];
    expect(gearTab.id == std::string(kPanelButtonTab) + "gear" && std::abs(gearTab.frame.x + gearTab.frame.width - (geared.size.x - p)) < 0.01f &&
               gearTab.hitFrame.x + gearTab.hitFrame.width == geared.size.x,
           "the gear in the right corner, its click area to the card's edge");
    const Tab* under = tabAt(*geared.tabs, { gearTab.frame.x + 2, gearTab.frame.y + 2 });
    expect(under && under->id == gearTab.id && gearTab.badge, "a point finds it; its panel's dot on it");
    bool gearDot = std::any_of(geared.badges.begin(), geared.badges.end(),
                               [&](Vec2 d) { return gearTab.frame.contains(d) && d.x > gearTab.frame.x + gearTab.frame.width / 2 && d.y <= gearTab.frame.y + gearTab.frame.height / 2; });
    expect(gearDot, "something new in Settings: a dot drawn in the gear's top-right corner (decisions 048, 113)");
    Panel quietGear = gear;
    quietGear.content.badge = false;
    Scene quietScene = layout(reduce(reduce(s, PanelPublished{ quietGear }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(!quietScene.tabs->buttons[0].badge && quietScene.badges.size() + 1 == geared.badges.size(), "nothing new: no dot");
    const Placed* gearIcon = find(geared, "tab/button/gear/lead");
    expect(gearIcon && std::get<Icon>(gearIcon->item).glyph == Glyph::Settings && gearIcon->muted, "the gear's glyph, grey until hovered");
    Scene gearOnly = layout(reduce(reduce(reduce(IslandState{}, PanelPublished{ panel("only") }, Ms{ 0 }), PanelPublished{ gear }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(gearOnly.tabs && gearOnly.tabs->tabs.size() == 1 && gearOnly.tabs->buttons.size() == 1, "one page with the gear: the row shows, so Settings is at hand");

    // Search in the left corner, the theme switch beside the gear (decisions 165, 170), whatever order they're published in.
    Panel moon{ .id = "theme", .content = { .label = "Dark mode" }, .providerId = "sys", .tapAction = "toggle",
                .rowButton = Glyph::Moon, .rowSpot = Panel::RowSpot::BesideCorner };
    Panel lens{ .id = "search", .content = { .label = "Search", .expandedCenter = { row({ Text{ "find" } }) } }, .providerId = "search",
                .rowButton = Glyph::Search, .rowSpot = Panel::RowSpot::LeftCorner };
    IslandState ends = s;
    for (const Panel& extra : { moon, lens, gear }) ends = reduce(ends, PanelPublished{ extra }, Ms{ 0 });
    ends = reduce(ends, PillClicked{}, Ms{ 0 });
    Scene ended = layout(ends, Ms{ 0 });
    auto buttonOf = [&](const std::string& id) {
        for (const Tab& b : ended.tabs->buttons)
            if (b.id == std::string(kPanelButtonTab) + id) return b;
        return Tab{};
    };
    Tab gearAt = buttonOf("gear"), moonAt = buttonOf("theme"), lensAt = buttonOf("search");
    expect(ended.tabs->buttons.size() == 3 && std::abs(gearAt.frame.x + gearAt.frame.width - (ended.size.x - p)) < 0.01f,
           "Settings stays in the right corner, though the theme switch was published first");
    expect(std::abs(moonAt.frame.x + moonAt.frame.width + t.tabIconGap - gearAt.frame.x) < 0.01f, "the theme switch right beside it");
    expect(lensAt.frame == Rect{ p, t.tabRowInset, t.rowButtonWidth, t.tabHeight } && lensAt.hitFrame.x == 0 && tabAt(*ended.tabs, { 1, 1 })->id == lensAt.id,
           "Search in the left corner, where Home's button was, its click area to the card's edge");
    expect(ended.activityId == "a" && ended.tabs->tabs.size() == 3, "neither is a page in the list, or the page the idle pill opens");
    for (const Tab& b : ended.tabs->buttons) {
        const Tab* found = tabAt(*ended.tabs, { b.frame.x + b.frame.width / 2, b.frame.y + b.frame.height / 2 });
        expect(found && found->id == b.id, "each button takes its own click: " + b.id);
    }
    expect(ended.tabs->tabs.front().frame.x >= lensAt.frame.x + lensAt.frame.width && ended.tabs->tabs.back().frame.x + t.tabIconSize <= moonAt.frame.x,
           "the list between them");
    float listLeft = ended.tabs->tabs.front().frame.x, listRight = ended.tabs->tabs.back().frame.x + ended.tabs->tabs.back().frame.width;
    expect(std::abs(listLeft - (ended.size.x - listRight)) < 0.01f, "...and centred on the card");
    Scene searching = layout(reduce(ends, CardOpened{ "search" }, Ms{ 0 }), Ms{ 0 });
    expect(searching.activityId == "search" && searching.tabs && !searching.tabs->active, "Search's button opens its card; no icon in the list is lit");

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
    IslandState synced = reduce(s, ClockSynced{ Ms{ (9 * 60 + 41) * 60000 + 15000 }, true }, Ms{ 0 });
    expect(!sceneDeadline(layout(reduce(synced, PillClicked{}, Ms{ 0 }), Ms{ 0 })), "nothing ticks on a card without a clock");

    // Countdown labels: time left without the "-".
    Timeline timer{ .duration = Ms{ 1500000 }, .anchoredAt = Ms{ 0 }, .playing = true };
    IslandState counting;
    counting = reduce(counting, ActivityStarted{ { .id = "t", .content = { .expandedBottom = { row({ TimeLabel{ .timeline = timer, .countdown = true } }) } } } }, Ms{ 0 });
    counting = reduce(counting, PillClicked{}, Ms{ 0 });
    Scene countdown = layout(counting, Ms{ 1 });
    const Placed* left = find(countdown, "expandedBottom/0/0");
    expect(left && std::get<Text>(left->item).value == "24:59" && std::get<Text>(left->item).centred, "countdown: 24:59, no minus, centred");
}

} // namespace cup::test
