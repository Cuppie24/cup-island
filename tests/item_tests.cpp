// Decision 010's items: the progress ring, Steps, and the accent colour.

#include "core/hit_test.h"
#include "core/item_metrics.h"
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
    float halo = 2 * t.stepHaloWidth; // room for the current step's ring
    expect(steps && steps->frame.height == t.stepDotSize + halo && steps->frame.width == 8 * t.stepDotSize + 7 * t.stepDotGap + halo,
           "steps: one dot per step, packed");
    Steps mixed{ .count = 3, .lengths = { StepLength::Long, StepLength::Dot, StepLength::Short } };
    expect(fixedWidthOf(mixed, 0, {}) == t.stepLongWidth + t.stepDotSize + t.stepShortWidth + 2 * t.stepDotGap + halo,
           "a long, a dot and a short step: each its length (decision 107)");

    // A centred card: one stack across the whole card; other slots are ignored.
    Content centred{ .card = CardLayout::Centred,
                     .expandedLeading = { row({ Image{} }) },
                     .expandedCenter = { row({ Text{ .value = "hi", .centred = true } }, Align::Center) } };
    Scene stack = layout(reduce(reduce(IslandState{}, ActivityStarted{ { .id = "c", .content = centred } }, Ms{ 0 }), PillClicked{}, Ms{ 0 }), Ms{ 0 });
    const Placed* hi = find(stack, "expandedCenter/0/0");
    expect(!find(stack, "expandedLeading/0/0"), "centred card: no art square");
    expect(hi && hi->frame.x == t.expandedPadding && hi->frame.width == stack.size.x - 2 * t.expandedPadding,
           "centred card: the centre slot spans the card");

    // A wavy bar (decision 167): at least its wave's swing tall, and its grown knob if it
    // can be dragged; its line a sine that straightens into its head.
    {
        Rect slot{ 0, 0, 200, 100 };
        Progress plain{ .timeline = running }, wavy{ .timeline = running, .wavy = true };
        Progress seekWavy{ .timeline = running, .seekable = true, .wavy = true };
        expect(heightOf(wavy, slot, 0) == waveSwing() && waveSwing() == t.progressHeight + 2 * t.progressWaveAmplitude &&
                   heightOf(wavy, slot, 0) > heightOf(plain, slot, 0),
               "a wavy bar is as tall as its wave's swing");
        expect(heightOf(seekWavy, slot, 0) >= t.progressHitHeight && heightOf(seekWavy, slot, 0) >= t.progressPinHeight * t.progressPinGrow,
               "a seekable wavy bar keeps its click height, with room for its knob grown");
        expect(isWavy(wavy) && !isWavy(plain) && !isWavy(Progress{ .timeline = running, .shape = ProgressShape::Ring, .wavy = true }),
               "only a bar is wavy, never a ring");
        float length = 5 * t.progressWaveLength, middle = 8;
        std::vector<Vec2> line = wavePoints(length, middle, 0), later = wavePoints(length, middle, 1.0f), flat = wavePoints(length, middle, 0, 0);
        bool inside = !line.empty(), backwards = true, level = true;
        float reach = 0;
        for (std::size_t i = 0; i < line.size(); ++i) {
            inside = inside && std::abs(line[i].y - middle) <= t.progressWaveAmplitude + 0.001f;
            backwards = backwards && (i == 0 || line[i].x < line[i - 1].x);
            level = level && flat[i].y == middle && flat[i].x == line[i].x;
            reach = std::max(reach, std::abs(line[i].y - middle));
        }
        expect(inside && backwards && line.front().x == 0 && line.back().x == -length, "the wave runs from its head back to its start, within its swing");
        expect(line.front().y == middle && std::abs(line[1].y - middle) < 0.1f && reach > t.progressWaveAmplitude * 0.95f,
               "the wave is straight at its head and swings fully further back");
        expect(later.size() == line.size() && flat.size() == line.size() && level, "every phase, and flat, has the same points: one turns into another");
        // Past the taper the line is a plain sine: a crest (the least y) is nearer the head a moment later.
        auto crestBack = [&](const std::vector<Vec2>& points) {
            std::size_t best = 0;
            for (std::size_t i = 0; i < points.size(); ++i)
                if (-points[i].x >= 2 * t.progressWaveLength && -points[i].x < 3 * t.progressWaveLength && (best == 0 || points[i].y < points[best].y)) best = i;
            return -points[best].x;
        };
        expect(crestBack(later) < crestBack(line), "the ripples run towards the head");
        expect(wavePoints(0, middle, 0).empty(), "no length: no wave");
    }

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
    // Proportional digits (Windows' UI font): sized with the widest one, not just "0".
    MeasureText wideFour = [](const Text& text) {
        float w = 0;
        for (char c : text.value) w += c == '4' ? 14.0f : 10.0f;
        return w;
    };
    Content fourDigits{ .expandedCenter = { row({ TimeLabel{ .timeline = { .duration = Ms{ 25 * 60000 }, .playing = true },
                                                            .countdown = true, .style = TextStyle::Display } }) } };
    IslandState fours = reduce(reduce(IslandState{}, ActivityStarted{ { .id = "f", .content = fourDigits } }, Ms{ 0 }), PillClicked{}, Ms{ 0 });
    Scene foursScene = layout(fours, Ms{ 0 }, wideFour);
    const Placed* fourLabel = find(foursScene, "expandedCenter/0/0");
    expect(fourLabel && fourLabel->frame.width == 4 * 14 + 10 + 1, "room for \"44:44\" when 4 is the widest digit");

    // No accent: none stamped (the renderer uses white).
    IslandState plain = reduce(IslandState{}, ActivityStarted{ { .id = "m", .content = { .compactCenter = { row({ Text{ "x" } }) } } } }, Ms{ 0 });
    Scene plainScene = layout(plain, Ms{ 0 });
    expect(find(plainScene, "compactCenter/0/0") && !find(plainScene, "compactCenter/0/0")->accent, "no accent: white");
}

} // namespace cup::test
