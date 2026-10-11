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

    // No accent: none stamped (the renderer uses white).
    IslandState plain = reduce(IslandState{}, ActivityStarted{ { .id = "m", .content = { .compactCenter = { row({ Text{ "x" } }) } } } }, Ms{ 0 });
    Scene plainScene = layout(plain, Ms{ 0 });
    expect(find(plainScene, "compactCenter/0/0") && !find(plainScene, "compactCenter/0/0")->accent, "no accent: white");
}

} // namespace cup::test
