// Content model: timeline math, time formatting, slot layout and hit-testing.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/split.h"
#include "core/tabs.h"
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

IslandState expandedWith(const Content& content)
{
    IslandState s;
    s = reduce(s, ActivityStarted{ { .id = "demo.media", .content = content, .providerId = "demo" } }, Ms{ 0 });
    return reduce(s, PillClicked{}, Ms{ 0 });
}

} // namespace

void runContentTests()
{
    // Timeline: extrapolates while playing, freezes when paused, clamps to duration.
    Timeline playing{ .position = Ms{ 10000 }, .duration = Ms{ 60000 }, .anchoredAt = Ms{ 1000 }, .playing = true };
    expect(playing.positionAt(Ms{ 3000 }) == Ms{ 12000 }, "position advances while playing");
    expect(playing.positionAt(Ms{ 999999 }) == Ms{ 60000 }, "position clamps to duration");
    Timeline paused = playing;
    paused.playing = false;
    expect(paused.positionAt(Ms{ 3000 }) == Ms{ 10000 }, "position frozen while paused");
    expect(playing.fractionAt(Ms{ 1000 }) > 0.166f && playing.fractionAt(Ms{ 1000 }) < 0.167f, "fraction");
    expect(Timeline{}.fractionAt(Ms{ 0 }) == 0.0f, "unknown duration -> 0");

    expect(formatDuration(Ms{ 0 }) == "0:00", "format 0");
    expect(formatDuration(Ms{ 83999 }) == "1:23", "format rounds down");
    expect(formatDuration(Ms{ 3723000 }) == "1:02:03", "format hours");

    // Expanded layout: art square, stacked text, bottom rows.
    Content media{
        .label = "Song",
        .expandedLeading = { row({ Image{} }) },
        .expandedCenter = { row({ Text{ "Song", TextStyle::Title } }), row({ Text{ "Artist" } }) },
        .expandedBottom = {
            row({ TimeLabel{ playing }, Progress{ playing, true }, TimeLabel{ playing, true } }),
            Row{ { Button{ Glyph::Previous, "previous" }, Button{ Glyph::Pause, "playPause" }, Button{ Glyph::Next, "next", false } },
                 Align::Center, /* loose */ true },
        },
    };
    IslandState s = expandedWith(media);
    Scene scene = layout(s, Ms{ 1000 });
    const Tokens& t = tokens();

    const Placed* art = find(scene, "expandedLeading/0/0");
    expect(art && art->frame == Rect{ t.expandedPadding, t.expandedPadding, t.expandedArtSize, t.expandedArtSize }, "art fills the leading square");

    const Placed* elapsed = find(scene, "expandedBottom/0/0");
    const Placed* bar = find(scene, "expandedBottom/0/1");
    const Placed* left = find(scene, "expandedBottom/0/2");
    expect(elapsed && std::get<Text>(elapsed->item).value == "0:10", "elapsed label formatted");
    expect(left && std::get<Text>(left->item).value == "-0:50" && std::get<Text>(left->item).alignEnd,
           "remaining label formatted, right-aligned");
    expect(bar && bar->frame.width == scene.size.x - 2 * t.expandedPadding - 2 * t.timeLabelWidth - 2 * t.itemSpacing,
           "progress stretches between the labels");

    const Placed* play = find(scene, "expandedBottom/1/1");
    expect(play && play->frame.x + play->frame.width / 2 == scene.size.x / 2, "button row is centred");
    for (const auto& p : scene.elements) {
        bool inside = p.frame.x >= 0 && p.frame.y >= 0 && p.frame.x + p.frame.width <= scene.size.x &&
                      p.frame.y + p.frame.height <= scene.size.y;
        expect(inside, "element inside the pill: " + p.key);
    }

    // Time labels tick once per second while playing, never while paused.
    auto deadline = sceneDeadline(scene);
    expect(deadline && *deadline > scene.now && *deadline <= scene.now + Ms{ 1000 }, "labels schedule the next second");
    expect(!sceneDeadline(layout(expandedWith({ .expandedBottom = { row({ TimeLabel{ paused } }) } }), Ms{ 0 })),
           "paused labels don't tick");

    // Hit-testing: buttons, disabled buttons, seek, and misses.
    auto centre = [](const Placed* p) { return Vec2{ p->frame.x + p->frame.width / 2, p->frame.y + p->frame.height / 2 }; };
    auto hit = hitTest(scene, centre(play));
    expect(hit && hit->action == Action{ "demo", "demo.media", "playPause", 0 } && hit->key == "expandedBottom/1/1",
           "button click routes to its provider");
    expect(!hitTest(scene, centre(find(scene, "expandedBottom/1/2"))), "disabled button is ignored");
    auto seek = hitTest(scene, { bar->frame.x + bar->frame.width / 4, centre(bar).y });
    expect(seek && seek->action.actionId == "seek" && seek->action.value > 0.24f && seek->action.value < 0.26f,
           "seek returns the fraction");
    expect(!hitTest(scene, centre(art)), "art is not clickable");
    Content clickableArt = media;
    clickableArt.expandedLeading = { row({ Image{ .actionId = "openSource" } }) };
    Scene artScene = layout(expandedWith(clickableArt), Ms{ 1000 });
    auto artHit = hitTest(artScene, centre(find(artScene, "expandedLeading/0/0")));
    expect(artHit && artHit->action.actionId == "openSource" && artHit->key == "expandedLeading/0/0", "art with an action is clickable");

    // Easy targets: neighbouring buttons' click areas meet, so the gap between them isn't dead.
    const Placed* previous = find(scene, "expandedBottom/1/0");
    expect(previous->frame.x + previous->frame.width + t.looseItemSpacing == play->frame.x, "loose rows use the wide gap");
    expect(previous->hitFrame.x + previous->hitFrame.width == play->hitFrame.x, "click areas meet in the middle of the gap");
    auto inGap = hitTest(scene, { play->frame.x - t.looseItemSpacing / 4, centre(play).y });
    expect(inGap && inGap->key == "expandedBottom/1/1", "a click just outside a button still hits it");
    expect(seekFraction(*bar, bar->frame.x - 50) == 0.0f && seekFraction(*bar, bar->frame.x + bar->frame.width + 50) == 1.0f,
           "scrubbing past the ends pins to them");

    // A trailing item sits at the end edge without pulling the centred group off centre.
    Content withSource = media;
    withSource.expandedBottom[1].trailing.push_back(Button{ Glyph::Source, "source" });
    Scene sourced = layout(expandedWith(withSource), Ms{ 1000 });
    const Placed* centred = find(sourced, "expandedBottom/1/1");
    const Placed* source = find(sourced, "expandedBottom/1/3");
    expect(centred && centred->frame == play->frame, "controls stay centred with a source switch");
    expect(source && source->frame.x + source->frame.width == sourced.size.x - t.expandedPadding,
           "source switch sits at the right edge, one padding in");
    auto sourceHit = hitTest(sourced, centre(source));
    expect(sourceHit && sourceHit->action.actionId == "source", "source switch is clickable");

    // Edge groups on both sides: the middle stays centred, the edges keep the padding,
    // and packed neighbours' click areas meet halfway.
    Content edges = withSource;
    edges.expandedBottom[1].trailing.insert(edges.expandedBottom[1].trailing.begin(), Button{ Glyph::Repeat, "repeat", true, false });
    edges.expandedBottom[1].leading.push_back(Button{ Glyph::Shuffle, "shuffle", true, true });
    Scene edged = layout(expandedWith(edges), Ms{ 1000 });
    const Placed* edgePlay = find(edged, "expandedBottom/1/1");
    const Placed* edgeRepeat = find(edged, "expandedBottom/1/3");
    const Placed* edgeSource = find(edged, "expandedBottom/1/4");
    const Placed* edgeShuffle = find(edged, "expandedBottom/1/5");
    expect(edgeShuffle && edgeShuffle->frame.x == t.expandedPadding, "leading edge item is one padding in");
    expect(edgeSource && edgeSource->frame.x + edgeSource->frame.width == edged.size.x - t.expandedPadding, "source still at the edge");
    expect(edgeRepeat && edgeRepeat->frame.x + edgeRepeat->frame.width + t.itemSpacing == edgeSource->frame.x, "edge group is packed");
    expect(edgeRepeat->hitFrame.x + edgeRepeat->hitFrame.width == edgeSource->hitFrame.x, "packed click areas meet halfway");
    float offCentre = edgePlay->frame.x + edgePlay->frame.width / 2 - edged.size.x / 2;
    expect(offCentre <= 0 && offCentre > -8, "middle group stays (almost) centred with both edges busy");
    const Placed* edgePrevious = find(edged, "expandedBottom/1/0");
    expect(edgePrevious->frame.x >= edgeShuffle->frame.x + edgeShuffle->frame.width + t.looseItemSpacing &&
           find(edged, "expandedBottom/1/2")->frame.x + find(edged, "expandedBottom/1/2")->frame.width + t.looseItemSpacing <= edgeRepeat->frame.x,
           "middle group never crowds the edge groups");

    // expanded.trailing: a button-sized square in the top-right corner; the centre makes room only when it's used.
    Content cornered = media;
    cornered.expandedTrailing = { row({ Button{ Glyph::Source, "source" } }) };
    Scene corneredScene = layout(expandedWith(cornered), Ms{ 1000 });
    const Placed* corner = find(corneredScene, "expandedTrailing/0/0");
    float cornerSize = t.buttonSize;
    expect(corner && corner->frame == Rect{ corneredScene.size.x - t.expandedPadding - cornerSize, t.expandedPadding, cornerSize, cornerSize },
           "corner button: one padding from the top and right edges");
    const Placed* narrowTitle = find(corneredScene, "expandedCenter/0/0");
    const Placed* wideTitle = find(scene, "expandedCenter/0/0");
    expect(narrowTitle && narrowTitle->frame.x + narrowTitle->frame.width + t.slotGap == corner->frame.x, "title stops a gap before the corner");
    expect(wideTitle && wideTitle->frame.x + wideTitle->frame.width == scene.size.x - t.expandedPadding, "without it the title runs to the edge");
    auto cornerHit = hitTest(corneredScene, centre(corner));
    expect(cornerHit && cornerHit->action.actionId == "source", "corner button is clickable");

    // Equal margins: the bottom row sits as far from the bottom as art from the side.
    expect(play->frame.y + play->frame.height == scene.size.y - t.expandedPadding, "expanded bottom margin = side margin");

    // Nested radius: rounded content is concentric with the pill's corner.
    expect(art->cornerRadius == pillCornerRadius(scene.size) - t.expandedPadding, "art radius = pill radius - padding");
    expect(play->cornerRadius == std::min(art->cornerRadius, t.buttonSize / 2), "button hover shape uses the same radius");
    expect(bar->cornerRadius == 0, "only art and buttons are rounded by layout");

    // Compact: equal padding on every side, a centre slot between the squares.
    IslandState compactState;
    compactState = reduce(compactState, ActivityStarted{ { .id = "demo.media", .content = {
        .compactLeading = { row({ Image{} }) },
        .compactCenter = { row({ Text{ .value = "Song - Artist", .marquee = true } }) },
        .compactTrailing = { row({ Waveform{ true } }, Align::Center) } } } }, Ms{ 0 });
    Scene compact = layout(compactState, Ms{ 0 });
    float inset = t.compactInset, side = compact.size.y - 2 * inset;
    const Placed* cArt = find(compact, "compactLeading/0/0");
    const Placed* cText = find(compact, "compactCenter/0/0");
    const Placed* cBars = find(compact, "compactTrailing/0/0");
    expect(cArt && cArt->frame == Rect{ inset, inset, side, side }, "compact art: same margin on every side");
    expect(cArt && cArt->cornerRadius == std::min(pillCornerRadius(compact.size) - inset, side / 2), "compact art radius is nested");
    expect(cBars && cBars->frame.x + cBars->frame.width / 2 == compact.size.x - inset - side / 2,
           "compact trailing square mirrors leading");
    expect(cText && cText->frame.x > cArt->frame.x + side && cText->frame.x + cText->frame.width < compact.size.x - inset - side,
           "compact title sits between the squares");

    expect(!scene.tabs, "no tabs with a single activity");

    // Split bubble: the 2nd activity's minimal slot (else compact leading), right of the pill.
    IslandState split;
    split = reduce(split, ActivityStarted{ { .id = "music", .content = { .compactLeading = { row({ Icon{ Glyph::Music } }) },
                                                                          .minimal = { row({ Image{} }) } } } }, Ms{ 0 });
    split = reduce(split, ActivityStarted{ { .id = "timer", .content = { .compactLeading = { row({ Icon{ Glyph::Warning } }) } } } }, Ms{ 0 });
    Scene rest = layout(split, Ms{ 0 });
    expect(rest.bubble && rest.bubble->activityId == "music" && rest.activityId == "timer", "bubble shows the 2nd activity");
    expect(rest.bubble->frame == bubbleHomeFrame(split) && rest.bubble->frame.x > rest.offset.x + rest.size.x, "bubble sits right of the pill");
    const Placed* bubbleArt = rest.bubble->elements.empty() ? nullptr : &rest.bubble->elements[0];
    expect(bubbleArt && std::holds_alternative<Image>(bubbleArt->item), "bubble uses the minimal slot");
    expect(bubbleArt && bubbleArt->frame == Rect{ t.compactInset, t.compactInset, t.bubbleSize - 2 * t.compactInset, t.bubbleSize - 2 * t.compactInset },
           "bubble content: same padding as compact, in bubble coordinates");
    float bubbleRadius = pillCornerRadius({ t.bubbleSize, t.bubbleSize });
    expect(bubbleArt && bubbleArt->cornerRadius == std::min(bubbleRadius - t.compactInset, bubbleArt->frame.width / 2),
           "bubble art radius is nested in the bubble's corner");
    expect(!rest.bubble->dragCentre && !rest.dropTarget, "bubble at rest");
    expect(!layout(reduce(split, ActivityEnded{ "music" }, Ms{ 0 }), Ms{ 0 }).bubble, "no bubble in Compact");
    Scene card = layout(reduce(split, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(!card.bubble && card.bubbleTuck.y == card.offset.y && card.bubbleTuck.x > card.offset.x &&
           card.bubbleTuck.x < card.offset.x + card.size.x,
           "tucked bubble waits on the top edge inside the pill, even under a tall card");
    IslandState timerSecond = reduce(split, ActivityPromoted{ "music" }, Ms{ 0 });
    Scene timerBubble = layout(timerSecond, Ms{ 0 });
    const Placed& icon = timerBubble.bubble->elements[0];
    expect(std::get<Icon>(icon.item).glyph == Glyph::Warning, "minimal falls back to compact leading");
    expect(icon.frame.x + icon.frame.width / 2 == t.bubbleSize / 2 && icon.frame.y + icon.frame.height / 2 == t.bubbleSize / 2,
           "an icon smaller than the bubble's square is centred in the bubble");
    const Placed* pillIcon = find(timerBubble, "compactLeading/0/0");
    expect(pillIcon && pillIcon->frame.x + pillIcon->frame.width / 2 == t.compactInset + (timerBubble.size.y - 2 * t.compactInset) / 2,
           "and in the pill's compact square");

    // Dragging: only the bubble moves. Over the pill, the pill is marked as the drop target; nothing swaps until release.
    Rect home = bubbleHomeFrame(split);
    Scene dragged = layout(reduce(split, BubbleDragged{ { -10, 5 } }, Ms{ 0 }), Ms{ 0 });
    expect(dragged.bubble->dragCentre == Vec2{ home.x + home.width / 2 - 10, home.y + home.height / 2 + 5 } &&
           dragged.activityId == "timer" && !dragged.dropTarget, "dragging: bubble follows the cursor, pill unchanged");
    Scene over = layout(reduce(split, BubbleDragged{ { -60, 0 } }, Ms{ 0 }), Ms{ 0 });
    expect(over.dropTarget && over.activityId == "timer" && over.bubble->activityId == "music", "over the pill: highlighted, not swapped");
    Scene committed = layout(reduce(reduce(split, BubbleDragged{ { -60, 0 } }, Ms{ 0 }), BubbleReleased{}, Ms{ 0 }), Ms{ 0 });
    expect(!committed.dropTarget && !committed.bubble->dragCentre && committed.activityId == "music" && committed.bubble->activityId == "timer",
           "letting go swaps them and the bubble goes home");
    Rect bounds = interactiveBounds(rest);
    expect(bounds.x == rest.offset.x && bounds.x + bounds.width == rest.bubble->frame.x + rest.bubble->frame.width,
           "interaction layer covers pill and bubble");
    expect(Rect{ rest.offset.x, rest.offset.y, rest.size.x, rest.size.y } == splitPillFrame(split), "split pill frame matches layout");

    // Expanded tabs (decision 007): one per activity along the top of the card.
    IslandState three;
    three = reduce(three, ActivityStarted{ { .id = "media", .content = { .label = "Song - Artist",
        .compactLeading = { row({ Image{} }) },
        .compactCenter = { row({ Text{ .value = "Song - Artist", .marquee = true } }) },
        .expandedLeading = { row({ Image{} }) } } } }, Ms{ 0 });
    three = reduce(three, ActivityStarted{ { .id = "timer", .content = { .label = "Timer",
        .compactLeading = { row({ Icon{ Glyph::Timer } }) } } } }, Ms{ 0 });
    three = reduce(three, ActivityStarted{ { .id = "sync", .content = { .label = "Sync" } } }, Ms{ 0 });
    Scene opened = layout(reduce(three, PillClicked{}, Ms{ 0 }), Ms{ 0 }); // opens the top one: sync
    expect(opened.tabs && opened.tabs->tabs.size() == 3, "a tab per activity, the shown one included");
    const TabStrip& strip = *opened.tabs;
    expect(strip.tabs[0].id == "media" && strip.tabs[2].id == "sync" && strip.active == 2,
           "tabs in start order (stable), the shown one marked active");
    expect(opened.size.y == t.expandedSize.y + t.tabHeight + t.slotGap, "the card grows by the strip");
    expect(opened.size.y <= t.canvas.y, "the taller card fits on the canvas");
    float groupLeft = strip.tabs.front().frame.x, groupRight = strip.tabs.back().frame.x + strip.tabs.back().frame.width;
    expect(strip.tabs[0].frame.width == t.tabMaxWidth || groupRight - groupLeft == opened.size.x - 2 * t.expandedPadding,
           "tabs use their full width unless they don't fit");
    expect(groupLeft - 0 == opened.size.x - groupRight, "the group is centred");
    expect(groupLeft >= t.expandedPadding && strip.tabs[0].frame.y + t.tabHeight == opened.size.y - t.expandedPadding,
           "strip along the bottom, keeping the card's padding");
    expect(strip.radius == std::min(pillCornerRadius(opened.size) - t.expandedPadding, t.tabHeight / 2), "tab corners nested");

    IslandState two;
    two = reduce(two, ActivityStarted{ { .id = "a" } }, Ms{ 0 });
    two = reduce(two, ActivityStarted{ { .id = "b" } }, Ms{ 0 });
    Scene pair = layout(reduce(two, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    expect(pair.tabs && pair.tabs->tabs[0].frame.width == t.tabMaxWidth, "few tabs: full width");
    expect(pair.tabs->tabs[0].frame.x == (pair.size.x - 2 * t.tabMaxWidth - t.tabGap) / 2, "few tabs: centred, not left-aligned");

    const Placed* artLead = find(opened, "tab/media/lead");
    const Placed* glyphLead = find(opened, "tab/timer/lead");
    const Placed* mediaTab = find(opened, "tab/media/title");
    const Placed* syncTab = find(opened, "tab/sync/title");
    expect(mediaTab && std::get<Text>(mediaTab->item).value == "Song - Artist" && !std::get<Text>(mediaTab->item).marquee,
           "tab title: the compact centre text, not swimming");
    expect(syncTab && std::get<Text>(syncTab->item).value == "Sync", "no compact centre: the activity's name");
    expect(artLead && artLead->cornerRadius > 0, "tab art radius nested");
    expect(glyphLead && glyphLead->frame.y + glyphLead->frame.height / 2 == strip.tabs[1].frame.y + strip.tabs[1].frame.height / 2,
           "tab glyph centred vertically");
    for (const Tab& tab : strip.tabs)
        for (const auto& p : opened.elements)
            if (p.key.rfind("tab/" + tab.id + "/", 0) == 0)
                expect(p.frame.x >= tab.frame.x && p.frame.x + p.frame.width <= tab.frame.x + tab.frame.width,
                       "tab content stays inside its tab: " + p.key);
    for (const auto& p : opened.elements)
        if (p.key.rfind("tab/", 0) != 0)
            expect(p.frame.y + p.frame.height <= strip.tabs[0].frame.y - t.slotGap, "card content sits above the strip: " + p.key);
    // Icon and title centred in the tab as one group, the title as wide as the real text.
    MeasureText fixed = [](const Text&) { return 40.0f; };
    Scene measured = layout(reduce(three, PillClicked{}, Ms{ 0 }), Ms{ 0 }, fixed);
    for (const Tab& tab : measured.tabs->tabs) {
        const Placed* lead = nullptr;
        const Placed* label = nullptr;
        for (const auto& p : measured.elements) {
            if (p.key == "tab/" + tab.id + "/lead") lead = &p;
            if (p.key == "tab/" + tab.id + "/title") label = &p;
        }
        float groupLeft = lead ? lead->frame.x - (lead->frame.width < t.tabHeight - 2 * t.compactInset
                                                      ? (t.tabHeight - 2 * t.compactInset - lead->frame.width) / 2 : 0)
                               : label->frame.x;
        float groupRight = label->frame.x + label->frame.width;
        float tabCentre = tab.frame.x + tab.frame.width / 2;
        expect(label && label->frame.width == 41, "title as wide as the measured text (+1 slack): " + tab.id);
        expect(std::abs((groupLeft + groupRight) / 2 - tabCentre) < 0.01f, "icon + title centred in the tab: " + tab.id);
    }
    MeasureText huge = [](const Text&) { return 1000.0f; };
    Scene squeezed = layout(reduce(three, PillClicked{}, Ms{ 0 }), Ms{ 0 }, huge);
    const Placed* longTitle = find(squeezed, "tab/media/title");
    const Tab& longTab = squeezed.tabs->tabs[0];
    expect(longTitle && longTitle->frame.x + longTitle->frame.width <= longTab.frame.x + longTab.frame.width - t.itemSpacing,
           "a title too long for its tab gets what's left (and ends in an ellipsis)");

    Vec2 onTimer{ strip.tabs[1].frame.x + 3, strip.tabs[1].frame.y + 3 };
    expect(tabAt(strip, onTimer) && tabAt(strip, onTimer)->id == "timer", "a point finds its tab");
    expect(!tabAt(strip, { 1, 1 }), "outside the strip: no tab");
    Scene mediaCard = layout(reduce(reduce(three, PillClicked{}, Ms{ 0 }), ActivitySwitched{ "media" }, Ms{ 0 }), Ms{ 0 });
    const Placed* mediaArt = find(mediaCard, "expandedLeading/0/0");
    expect(mediaArt && mediaArt->frame.y == t.expandedPadding, "card content keeps its place at the top");
    for (const auto& p : mediaCard.elements)
        if (p.key.rfind("tab/", 0) != 0)
            expect(p.frame.y + p.frame.height <= mediaCard.tabs->tabs[0].frame.y - t.slotGap, "media card content above the strip: " + p.key);
    expect(mediaCard.tabs->tabs[0].id == "media" && mediaCard.tabs->active == 0u, "switching doesn't reorder tabs");

    // A full media card (progress + controls) with tabs: the bottom rows move up to clear the strip.
    IslandState busy;
    busy = reduce(busy, ActivityStarted{ { .id = "timer" } }, Ms{ 0 });
    busy = reduce(busy, ActivityStarted{ { .id = "demo.media", .content = media, .providerId = "demo" } }, Ms{ 0 });
    Scene busyCard = layout(reduce(busy, PillClicked{}, Ms{ 0 }), Ms{ 1000 });
    const Placed* busyPlay = find(busyCard, "expandedBottom/1/1");
    expect(busyCard.tabs && busyPlay && busyPlay->frame.y + busyPlay->frame.height <= busyCard.tabs->tabs[0].frame.y - t.slotGap,
           "controls sit above the tab strip");
    expect(busyPlay && busyPlay->frame == play->frame, "and exactly where they are without tabs");
}

} // namespace cup::test
