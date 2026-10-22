// Unit tests for the parts of core/ that aren't state rules. State rules
// live in scenarios/*.scenario instead.

#include "core/easing.h"
#include "core/island.h"
#include "core/layout.h"
#include "core/split.h"
#include "core/text_edit.h"
#include "core/tokens.h"
#include "expect.h"

#include <algorithm>
#include <cmath>

namespace cup::test {

void runUnitTests()
{
    // Pixel snapping (decision 071): each edge to the nearest pixel, so the centre stays (rounding the size up drifted it low).
    Rect snapped = snapToPixels({ 10.4f, 20.6f, 27.2f, 27.2f });
    expect(snapped == Rect{ 10, 21, 28, 27 }, "both edges to the nearest pixel");
    expect(std::abs((snapped.y + snapped.height / 2) - (20.6f + 13.6f)) <= 0.5f, "the centre within half a pixel of where it was");

    // Offsets between the draft's UTF-8 bytes and DirectWrite's UTF-16 units (decision 043).
    std::string mixed = "aé€😀b"; // 1, 2, 3 and 4 bytes; the emoji is a surrogate pair
    expect(utf16Index(mixed, 0) == 0 && utf16Index(mixed, 1) == 1 && utf16Index(mixed, 3) == 2 && utf16Index(mixed, 6) == 3,
           "utf16Index counts one unit per BMP character");
    expect(utf16Index(mixed, 10) == 5 && utf16Index(mixed, 11) == 6, "...and two for one outside it");
    expect(utf8Offset(mixed, 3) == 6 && utf8Offset(mixed, 5) == 10 && utf8Offset(mixed, 6) == 11 && utf8Offset(mixed, 99) == 11,
           "utf8Offset maps back, clamped to the end");
    expect(utf8Offset(mixed, 4) == 10, "half a surrogate pair rounds to after the character");

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

    // An alert's icon (an update, a battery warning): centred in its leading
    // square, so it sits in the middle of the island's rounded end, as far
    // from the top and bottom as from the side (decision 006).
    IslandState alerting;
    alerting = reduce(alerting, AlertPosted{ { .content = { .label = "Update", .alertLeading = { row({ Icon{ Glyph::Download } }) },
                                                            .alertCenter = { row({ Text{ "Update available" } }) } } } }, Ms{ 0 });
    Scene alert = layout(alerting, Ms{ 0 });
    const Placed& icon = alert.elements[0];
    float fromSide = icon.frame.x, fromTop = icon.frame.y;
    expect(alert.mode == Mode::Alert && icon.key == "alertLeading/0/0" &&
           std::abs(icon.frame.x + icon.frame.width / 2 - alert.size.y / 2) < 0.5f && std::abs(fromSide - fromTop) < 0.5f,
           "an alert's icon sits in the middle of the rounded end, not against its edge");
}

} // namespace cup::test
