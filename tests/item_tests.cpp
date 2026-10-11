// Decision 010's items: the progress ring, Steps, and the accent colour.

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

} // namespace

void runItemTests()
{
    const Tokens& t = tokens();
    Timeline running{ .duration = Ms{ 60000 }, .anchoredAt = Ms{ 0 }, .playing = true };
    Color red{ 255, 99, 71 };

    // Compact: a ring fills the trailing square, like art; a centred countdown between.
    Content content{
        .label = "timer",
        .accent = red,
        .compactCenter = { row({ TimeLabel{ .timeline = running, .countdown = true, .style = TextStyle::Body } }, Align::Center) },
        .compactTrailing = { row({ Progress{ .timeline = running, .seekable = true, .shape = ProgressShape::Ring } }) },
        .expandedCenter = { row({ Steps{ 8, 3 } }) },
    };
    IslandState s = reduce(IslandState{}, ActivityStarted{ { .id = "timer", .content = content, .providerId = "p" } }, Ms{ 0 });
    Scene compact = layout(s, Ms{ 0 });
    float side = compact.size.y - 2 * t.compactInset;
    const Placed* ring = find(compact, "compactTrailing/0/0");
    expect(ring && ring->frame == Rect{ compact.size.x - t.compactInset - side, t.compactInset, side, side },
           "a ring fills the compact square");
    expect(ring && ring->accent == red, "items carry their content's accent");
    expect(!hitTest(compact, { ring->frame.x + side / 2, ring->frame.y + side / 2 }), "a ring is never seekable");
    const Placed* clock = find(compact, "compactCenter/0/0");
    const Text* digits = clock ? std::get_if<Text>(&clock->item) : nullptr;
    expect(digits && digits->value == "1:00" && digits->centred && digits->style == TextStyle::Body, "countdown: centred, in its style");
    expect(std::abs(clock->frame.x + clock->frame.width / 2 - compact.size.x / 2) < 0.01f, "in the middle of the pill");

    // Steps: a fixed-width row of dots.
    Scene card = layout(reduce(s, PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Placed* steps = find(card, "expandedCenter/0/0");
    expect(steps && steps->frame.height == t.stepDotSize && steps->frame.width == 8 * t.stepDotSize + 7 * t.stepDotGap,
           "steps: one dot per step, packed");

    // A centred card: one stack across the whole card; other slots are ignored.
    Content centred{ .card = CardLayout::Centred,
                     .expandedLeading = { row({ Image{} }) },
                     .expandedCenter = { row({ Text{ .value = "hi", .centred = true } }, Align::Center) } };
    Scene stack = layout(reduce(reduce(IslandState{}, ActivityStarted{ { .id = "c", .content = centred } }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Placed* hi = find(stack, "expandedCenter/0/0");
    expect(!find(stack, "expandedLeading/0/0"), "centred card: no art square");
    expect(hi && hi->frame.x == t.expandedPadding && hi->frame.width == stack.size.x - 2 * t.expandedPadding,
           "centred card: the centre slot spans the card");

    // RowSpan: a row spans its neighbour's content instead of the slot.
    Content matched{ .card = CardLayout::Centred,
                     .expandedCenter = { row({ Button{ Glyph::Play, "a" }, Button{ Glyph::Stop, "b" } }, Align::Center),
                                         Row{ .items = { Progress{ .timeline = running } }, .span = RowSpan::MatchAbove },
                                         row({ Progress{ .timeline = running } }),
                                         Row{ .items = { Progress{ .timeline = running } }, .span = RowSpan::MatchBelow },
                                         row({ Steps{ 4, 1 } }, Align::Center) } };
    Scene matchedScene = layout(reduce(reduce(IslandState{}, ActivityStarted{ { .id = "m", .content = matched } }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Placed* firstButton = find(matchedScene, "expandedCenter/0/0");
    const Placed* lastButton = find(matchedScene, "expandedCenter/0/1");
    const Placed* narrowBar = find(matchedScene, "expandedCenter/1/0");
    const Placed* wideBar = find(matchedScene, "expandedCenter/2/0");
    expect(narrowBar && narrowBar->frame.x == firstButton->frame.x &&
           narrowBar->frame.x + narrowBar->frame.width == lastButton->frame.x + lastButton->frame.width,
           "MatchAbove: the bar spans exactly the row above's content");
    expect(wideBar && wideBar->frame.width == matchedScene.size.x - 2 * t.expandedPadding, "without it, the slot's width");
    const Placed* belowBar = find(matchedScene, "expandedCenter/3/0");
    const Placed* stepsBelow = find(matchedScene, "expandedCenter/4/0");
    expect(belowBar && stepsBelow && belowBar->frame.x == stepsBelow->frame.x && belowBar->frame.width == stepsBelow->frame.width,
           "MatchBelow: the bar spans exactly the row below's content");

    // Large labels are as wide as their widest digits, so they don't jump while ticking.
    MeasureText perChar = [](const Text& text) { return static_cast<float>(text.value.size()) * 10.0f; };
    Content big{ .expandedCenter = { row({ TimeLabel{ .timeline = running, .countdown = true, .style = TextStyle::Display } }) } };
    IslandState bigState = reduce(reduce(IslandState{}, ActivityStarted{ { .id = "b", .content = big } }, Ms{ 0 }), PillClicked{}, Ms{ 0 });
    Scene now0 = layout(bigState, Ms{ 0 }, perChar);
    const Placed* bigLabel = find(now0, "expandedCenter/0/0");
    Scene later = layout(bigState, Ms{ 50000 }, perChar);
    expect(bigLabel && bigLabel->frame.width == 41, "display label: as wide as \"0:00\" (+1)");
    expect(find(later, "expandedCenter/0/0")->frame.width == 41, "and stays that wide as it ticks");

    // No accent: none stamped (the renderer uses white).
    IslandState plain = reduce(IslandState{}, ActivityStarted{ { .id = "m", .content = { .compactCenter = { row({ Text{ "x" } }) } } } }, Ms{ 0 });
    Scene plainScene = layout(plain, Ms{ 0 });
    expect(find(plainScene, "compactCenter/0/0") && !find(plainScene, "compactCenter/0/0")->accent, "no accent: white");
}

} // namespace cup::test
