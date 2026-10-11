#pragma once
// Every number that controls how the island looks and feels. The defaults
// below are the fallback; at runtime the app loads assets/tokens.json over
// them and reloads it on save (core/tokens_json.h). Nothing else hard-codes
// sizes or timings.

#include "core/content.h"
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
    Vec2 canvas{ 420, 220 }; // room for the largest pill: Expanded with tabs

    // Pill size per mode (PS §3.1). Height 0 = tucked into the screen edge.
    Vec2 idleSize{ 160, 28 };
    Vec2 compactSize{ 260, 32 };
    Vec2 alertSize{ 320, 64 };
    Vec2 expandedSize{ 380, 154 };
    Vec2 receptiveSize{ 380, 84 };
    Vec2 hiddenSize{ 160, 0 };
    float maxCornerRadius = 28; // bottom corners: half the height, capped, so big shapes aren't blobs

    // Content layout inside the pill (PS §5.1 slots). Fonts are in pixels.
    // Each mode has one padding, used on every side; rounded content inside
    // gets the nested radius (pill radius - padding), so corners stay concentric.
    float compactInset = 4;     // compact padding
    float alertPadding = 8;
    float expandedPadding = 12;
    float expandedArtSize = 64; // expanded leading square
    float slotGap = 10;         // between leading and center/bottom slots
    float rowSpacing = 4;
    float itemSpacing = 8;
    float looseItemSpacing = 24; // Row::loose, e.g. media controls
    float titleFontSize = 15;
    float bodyFontSize = 13;
    float captionFontSize = 11;
    float displayFontSize = 34; // TextStyle::Display: a timer as the card's centre
    float iconSize = 18;
    float buttonSize = 36;        // hover circle; the click area also takes in half the gaps around it
    float buttonGlyphSize = 16;
    float buttonPressScale = 0.85f;
    float progressHeight = 4;
    float progressHitHeight = 16; // taller than drawn, so it's easy to click
    float progressKnobSize = 12;  // the scrub handle shown on hover
    float timeLabelWidth = 48;
    float ringThickness = 3;      // progress ring stroke (decision 010)
    float stepDotSize = 9;        // Steps: one dot per step
    float stepDotGap = 8;

    // Hover on buttons and the progress bar (decision 002: content only, not the pill).
    Ms hoverFadeDuration{ 120 };

    // Swimming (marquee) text: pauses at the start, then scrolls left in a loop.
    float marqueeSpeed = 30;      // pixels per second
    Ms marqueePause{ 1800 };
    float marqueeGap = 32;        // between the end of the text and its repeat
    float marqueeFadeWidth = 12;  // soft edges where text enters and leaves

    // Waveform ("now playing" bars). Paused, the bars rest as dots.
    std::size_t waveformBarCount = 3;
    float waveformBarWidth = 3;
    float waveformBarGap = 3;
    Ms waveformPeriod{ 1100 };    // one bar's rise-and-fall; neighbours are a bit slower

    // Split bubble (PS §3.1) and dragging it onto the pill to swap (decision 007).
    float bubbleSize = 32;        // width and height; shaped like the pill (straight top, rounded bottom)
    float bubbleGap = 8;          // between the pill's right edge and the bubble at rest
    float dragStartDistance = 4;  // a press on the bubble that moves less is a click
    float bridgeThickness = 20;   // liquid neck between pill and bubble, at its thickest
    float bridgeReach = 6;        // edge gap at which the neck has thinned to nothing (< bubbleGap: none at rest)

    // Expanded's tabs: one per activity along the bottom of the card (decision 007).
    float tabHeight = 28;
    float tabGap = 4;
    float tabMaxWidth = 140;      // each tab's width; the group is centred, and shrinks only when they don't fit
    float homeButtonWidth = 44;   // Home's icon-only button, apart at the left (decision 009)

    // Home: a grid of panel tiles in the Expanded card (decision 009).
    std::size_t homeColumns = 4;  // a tile is one column wide, or two (Panel::wide)
    float tileHeight = 58;
    float tileGap = 8;
    float tilePadding = 8;        // inside a tile, on every side

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
    // After any app starts/stops playing, ask Windows again this much later:
    // its "current session" can switch slightly after the playback event.
    Ms mediaSessionRecheck{ 300 };

    // Colours: every colour the island draws. tokens.json writes [r, g, b] or
    // [r, g, b, alpha], channels 0..255 and alpha 0..1, like CSS rgba().
    Color islandColor{ 0, 0, 0 };
    Color islandHighlightColor{ 46, 46, 46 }; // the pill while the dragged bubble is over it (decision 007)
    Color titleTextColor{ 255, 255, 255 };
    Color bodyTextColor{ 255, 255, 255, 179 };
    Color captionTextColor{ 255, 255, 255, 140 }; // also Digits
    Color iconColor{ 255, 255, 255, 230 };
    Color buttonColor{ 255, 255, 255, 242 };       // glyph, and a toggle's "on" dot
    Color buttonOffColor{ 255, 255, 255, 128 };    // a toggle that's off
    Color buttonDisabledColor{ 255, 255, 255, 77 };
    Color buttonHoverColor{ 255, 255, 255, 41 };   // the shape behind a hovered button
    Color trackColor{ 255, 255, 255, 61 };         // a progress bar's or ring's empty part
    Color liveColor{ 255, 255, 255, 230 };         // progress fill, waveform: when the content has no accent
    Color knobColor{ 255, 255, 255 };
    Color stepDoneColor{ 255, 255, 255, 191 };
    Color stepTodoColor{ 255, 255, 255, 51 };
    Color tabSelectedColor{ 255, 255, 255, 41 };
    Color hoverColor{ 255, 255, 255, 20 };         // a tab or tile under the pointer
    Color tileColor{ 255, 255, 255, 20 };          // Home's tile backgrounds
    Color imagePlaceholderColor{ 255, 255, 255, 31 }; // art that hasn't arrived
    Color pomodoroFocusColor{ 255, 99, 71 };       // accents (decision 010)
    Color pomodoroBreakColor{ 52, 199, 89 };

    // Pomodoro (decision 009): focus rounds with short breaks, a long break after the last round.
    Ms pomodoroFocus{ 1500000 };
    Ms pomodoroBreak{ 300000 };
    Ms pomodoroLongBreak{ 900000 };
    std::size_t pomodoroRounds = 4;

    // Home's "Copy logs" tile: how long it says "Copied" after a tap.
    Ms logsNote{ 2000 };

    bool operator==(const Tokens&) const = default;
};

// The active tokens: defaults until the app applies a loaded tokens.json.
const Tokens& tokens();

// App only, on the UI thread. The one piece of global state in core/: config
// that everything reads and only the tokens.json reload writes.
void setTokens(const Tokens& tokens);

} // namespace cup
