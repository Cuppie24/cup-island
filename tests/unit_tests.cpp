// Unit tests for the parts of core/ that aren't state rules. State rules
// live in scenarios/*.scenario instead.

#include "core/easing.h"
#include "core/island.h"
#include "core/layout.h"
#include "core/split.h"
#include "core/tokens.h"
#include "expect.h"

#include <algorithm>

namespace cup::test {

void runUnitTests()
{
    // Easing: pinned endpoints, monotonic, and ease-out (ahead of linear).
    Bezier ease = tokens().morphEase;
    expect(cubicBezierEase(ease, 0.0f) == 0.0f, "ease(0) == 0");
    expect(cubicBezierEase(ease, 1.0f) == 1.0f, "ease(1) == 1");
    expect(cubicBezierEase(ease, 0.5f) > 0.5f, "ease-out is ahead of linear at 0.5");
    float previous = 0.0f;
    bool monotonic = true;
    for (int i = 1; i <= 100; ++i) {
        float v = cubicBezierEase(ease, i / 100.0f);
        monotonic = monotonic && v >= previous;
        previous = v;
    }
    expect(monotonic, "ease is monotonic");

    // Idle island has no deadline: nothing ticks, zero cost (TR §5, §8).
    IslandState idle;
    expect(!nextDeadline(idle).has_value(), "idle island schedules no ticks");

    // Layout: pill is centered in the canvas and hangs from the top edge.
    Scene scene = layout(idle, Ms{ 0 });
    expect(scene.size == tokens().idleSize, "idle uses idle size");
    expect(scene.offset.x * 2 + scene.size.x == tokens().canvas.x, "pill is horizontally centered");
    expect(scene.offset.y == 0, "pill is flush with the top edge");
    // The flared top corners stick out beside the widest pill (decision 025): still on the canvas.
    float widest = std::max({ tokens().expandedSize.x, tokens().receptiveSize.x, tokens().alertSize.x, tokens().compactSize.x });
    expect(widest + 2 * tokens().flareRadius <= tokens().canvas.x, "the widest pill's flared corners fit the canvas");

    // Split: the gap is the two flares, so they meet in one arch (decision 025).
    expect(splitGap() == 2 * tokens().flareRadius, "the bubble sits two flares from the pill");
    {
        Tokens saved = tokens(), flat = saved;
        flat.flareRadius = 0;
        setTokens(flat);
        expect(splitGap() == flat.bubbleGap, "without flares the gap is bubbleGap");
        setTokens(saved);
    }

    // Expanded shows the opened activity's content.
    IslandState s;
    s = reduce(s, ActivityStarted{ { .id = "media", .content = { .label = "Song", .expandedCenter = { row({ Text{ "Song" } }) } } } }, Ms{ 0 });
    s = reduce(s, PillClicked{}, Ms{ 10 });
    scene = layout(s, Ms{ 10 });
    expect(scene.mode == Mode::Expanded, "click on compact expands");
    expect(scene.label == "Song", "expanded shows the activity's content");
    expect(scene.elements.size() == 1 && scene.elements[0].key == "expandedCenter/0/0", "one placed text");
}

} // namespace cup::test
