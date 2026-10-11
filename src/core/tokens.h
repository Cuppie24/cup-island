#pragma once
// Every number that controls how the island looks and feels. The defaults
// below are the fallback; at runtime the app loads assets/tokens.json over
// them and reloads it on save (core/tokens_json.h). Nothing else hard-codes
// sizes or timings.

#include "core/geometry.h"

#include <chrono>
#include <cstddef>

namespace cup {

using Ms = std::chrono::milliseconds;

// CSS-style cubic-bezier(c1.x, c1.y, c2.x, c2.y): paste from cubic-bezier.com.
struct Bezier {
    Vec2 c1;
    Vec2 c2;
    constexpr bool operator==(const Bezier&) const = default;
};

struct Tokens {
    // Render layer: sized once to the largest pill plus press-overshoot room,
    // never resized (TR §5, §9.1). Only read at startup.
    Vec2 canvas{ 420, 180 };

    // Pill size per mode (PS §3.1). Height 0 = tucked into the screen edge.
    Vec2 idleSize{ 160, 28 };
    Vec2 compactSize{ 220, 32 };
    Vec2 alertSize{ 320, 64 };
    Vec2 expandedSize{ 380, 150 };
    Vec2 receptiveSize{ 380, 84 };
    Vec2 hiddenSize{ 160, 0 };
    float maxCornerRadius = 28; // bottom corners: half the height, capped, so big shapes aren't blobs

    // Content layout inside the pill (PS §5.1 slots). Fonts are in pixels.
    float compactInset = 4;     // compact leading/trailing squares sit this far in
    float alertPadding = 8;
    float expandedPadding = 12;
    float expandedArtSize = 64; // expanded leading square
    float slotGap = 10;         // between leading and center/bottom slots
    float rowSpacing = 4;
    float itemSpacing = 8;
    float titleFontSize = 15;
    float bodyFontSize = 13;
    float captionFontSize = 11;
    float iconSize = 18;
    float buttonSize = 32;
    float progressHeight = 4;
    float progressHitHeight = 14; // taller than drawn, so it's easy to click
    float timeLabelWidth = 48;
    float artCornerRadius = 6;

    // Morph between modes: ease-out, 250-350 ms budget (TR §5).
    Ms morphDuration{ 300 };
    Bezier morphEase{ { 0.16f, 1.0f }, { 0.30f, 1.0f } };

    // Content crossfades slightly after the pill starts morphing.
    Ms contentFadeDuration{ 200 };
    Ms contentFadeDelay{ 100 };

    // Press feedback: squash on mouse-down, spring back with overshoot (y > 1) on release.
    float pressScale = 0.96f;
    Ms pressDownDuration{ 100 };
    Bezier pressDownEase{ { 0.4f, 0.0f }, { 0.2f, 1.0f } };
    Ms pressReleaseDuration{ 220 };
    Bezier pressReleaseEase{ { 0.34f, 1.56f }, { 0.64f, 1.0f } };

    // Arbitration (PS §6).
    Ms alertDuration{ 3000 };
    Ms expandedIdleTimeout{ 8000 };
    Ms heldAlertMaxAge{ 30000 };
    std::size_t alertQueueCapacity = 5;

    // Media (PS §10): a paused track stays on the island this long, then ends.
    Ms mediaPausedLinger{ 300000 };

    bool operator==(const Tokens&) const = default;
};

// The active tokens: defaults until the app applies a loaded tokens.json.
const Tokens& tokens();

// App only, on the UI thread. The one piece of global state in core/: config
// that everything reads and only the tokens.json reload writes.
void setTokens(const Tokens& tokens);

} // namespace cup
