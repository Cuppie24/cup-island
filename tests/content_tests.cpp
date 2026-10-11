// Content model: timeline math, time formatting, slot layout and hit-testing.

#include "core/hit_test.h"
#include "core/layout.h"
#include "core/split.h"
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
            row({ Button{ Glyph::Previous, "previous" }, Button{ Glyph::Pause, "playPause" }, Button{ Glyph::Next, "next", false } }, Align::Center),
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
    expect(!find(scene, "switcher/next"), "no switcher with a single activity");
    expect(!islandEvent(hit->action), "provider actions are not island events");

    // Expanded switcher (decision 006): "next" always, "promote" unless it's already the pill.
    s = reduce(s, ActivityStarted{ { .id = "demo.timer", .content = { .expandedCenter = { row({ Text{ "Timer" } }) } } } }, Ms{ 0 });
    s = reduce(s, PillClicked{}, Ms{ 0 });
    s = reduce(s, PillClicked{}, Ms{ 0 }); // collapse, then open the top one: the timer
    scene = layout(s, Ms{ 0 });
    const Placed* next = find(scene, "switcher/next");
    expect(next && next->frame.x + next->frame.width == scene.size.x - t.expandedPadding, "switcher sits at the right edge");
    expect(!find(scene, "switcher/promote"), "no promote button on the activity that has the pill");
    const Placed* title = find(scene, "expandedCenter/0/0");
    expect(title && title->frame.x + title->frame.width <= next->frame.x - t.itemSpacing, "center text stays clear of the switcher");

    auto paged = hitTest(scene, centre(next));
    expect(paged && islandEvent(paged->action) && std::holds_alternative<ExpandedPaged>(*islandEvent(paged->action)),
           "next button pages");
    s = reduce(s, *islandEvent(paged->action), Ms{ 0 });
    scene = layout(s, Ms{ 0 });
    const Placed* promote = find(scene, "switcher/promote");
    auto promoted = promote ? hitTest(scene, centre(promote)) : std::nullopt;
    auto event = promoted ? islandEvent(promoted->action) : std::nullopt;
    expect(event && std::get_if<ActivityPromoted>(&*event) && std::get<ActivityPromoted>(*event).id == "demo.media",
           "promote button picks the shown activity");
    for (const auto& p : scene.elements)
        expect(p.frame.x >= 0 && p.frame.x + p.frame.width <= scene.size.x && p.frame.y + p.frame.height <= scene.size.y,
               "switcher element inside the pill: " + p.key);

    // Split bubble: the 2nd activity's minimal slot (else compact leading), right of the pill.
    IslandState split;
    split = reduce(split, ActivityStarted{ { .id = "music", .content = { .compactLeading = { row({ Icon{ Glyph::Music } }) },
                                                                          .minimal = { row({ Icon{ Glyph::Pause } }) } } } }, Ms{ 0 });
    split = reduce(split, ActivityStarted{ { .id = "timer", .content = { .compactLeading = { row({ Icon{ Glyph::Warning } }) } } } }, Ms{ 0 });
    Scene rest = layout(split, Ms{ 0 });
    expect(rest.bubble && rest.bubble->activityId == "music" && rest.activityId == "timer", "bubble shows the 2nd activity");
    expect(rest.bubble->frame == bubbleHomeFrame() && rest.bubble->frame.x > rest.offset.x + rest.size.x, "bubble sits right of the pill");
    expect(rest.bubble->elements.size() == 1 && std::get<Icon>(rest.bubble->elements[0].item).glyph == Glyph::Pause,
           "bubble uses the minimal slot");
    expect(rest.bubble->elements[0].frame.x >= 0 && rest.bubble->elements[0].frame.x + rest.bubble->elements[0].frame.width <= t.bubbleSize,
           "bubble content is in bubble coordinates");
    expect(!rest.drag, "bubble at rest");
    expect(!layout(reduce(split, ActivityEnded{ "music" }, Ms{ 0 }), Ms{ 0 }).bubble, "no bubble in Compact");

    Scene dragged = layout(reduce(split, BubbleDragged{ { -10, 5 } }, Ms{ 0 }), Ms{ 0 });
    Rect home = bubbleHomeFrame();
    expect(dragged.drag && dragged.drag->activityId == "music" &&
           dragged.drag->centre == Vec2{ home.x + home.width / 2 - 10, home.y + home.height / 2 + 5 } && dragged.activityId == "timer",
           "dragging: the bubble's shape follows the cursor, pill unchanged");

    // Over the pill: preview the swap. Pill shows the dragged activity, the bubble the old top, at rest.
    Scene preview = layout(reduce(split, BubbleDragged{ { -60, 0 } }, Ms{ 0 }), Ms{ 0 });
    expect(preview.activityId == "music" && preview.bubble->activityId == "timer", "preview swaps roles");
    expect(preview.drag && preview.drag->activityId == "music", "the dragged shape still follows the cursor while previewing");
    expect(std::get<Icon>(preview.bubble->elements[0].item).glyph == Glyph::Warning, "minimal falls back to compact leading");
    Scene committed = layout(reduce(reduce(split, BubbleDragged{ { -60, 0 } }, Ms{ 0 }), BubbleReleased{}, Ms{ 0 }), Ms{ 0 });
    expect(!committed.drag && committed.activityId == preview.activityId && committed.bubble->activityId == preview.bubble->activityId,
           "letting go keeps exactly what the preview showed");
    Rect bounds = interactiveBounds(rest);
    expect(bounds.x == rest.offset.x && bounds.x + bounds.width == rest.bubble->frame.x + rest.bubble->frame.width,
           "interaction layer covers pill and bubble");
    expect(Rect{ preview.offset.x, preview.offset.y, preview.size.x, preview.size.y } == splitPillFrame(), "split pill frame matches layout");
}

} // namespace cup::test
