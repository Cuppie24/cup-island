// Content model: timeline math, time formatting, slot layout and hit-testing.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/tokens.h"
#include "expect.h"

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
}

} // namespace cup::test
