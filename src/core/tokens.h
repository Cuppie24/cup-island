#pragma once
// Every number that controls how the island looks and feels. The defaults
// below are the fallback; at runtime the app loads assets/tokens.json over
// them and reloads it on save (core/tokens_json.h). Nothing else hard-codes
// sizes or timings.

#include "core/content.h"
#include "core/geometry.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace cup {

using Ms = std::chrono::milliseconds;

// CSS-style cubic-bezier(c1.x, c1.y, c2.x, c2.y): paste from cubic-bezier.com.
struct Bezier {
    Vec2 c1;
    Vec2 c2;
    constexpr bool operator==(const Bezier&) const = default;
};

// An app theme (decision 022): a name, and the colour tokens it changes from
// the values below; every other colour stays as written. Dark and light.
struct ThemeColor {
    std::string token; // a colour token's name: "islandColor"
    Color color;
    bool operator==(const ThemeColor&) const = default;
};
struct Theme {
    std::string name;
    std::vector<ThemeColor> colors;
    bool operator==(const Theme&) const = default;
};

struct Tokens {
    // Render layer: as wide as the island's monitor, so the island can move
    // along its top edge (decision 026), and this tall (TR §5, §9.1). The
    // width here is only a minimum, and the width layout() uses until the
    // platform says (tests).
    Vec2 canvas{ 440, 420 }; // room for the largest pill (a Centred card at its tallest, with tabs) and its shadow

    // One knob for the island's overall size: every length below is
    // multiplied by it, on top of the display's scaling (decision 017).
    // 1 = the sizes as written; below 1 = more compact.
    float islandScale = 0.85f;

    // Pill size per mode (PS §3.1). Height 0 = tucked into the screen edge.
    Vec2 idleSize{ 160, 28 };
    Vec2 compactSize{ 260, 32 };
    Vec2 alertSize{ 320, 64 };
    Vec2 expandedSize{ 380, 159 };
    Vec2 receptiveSize{ 380, 84 };
    Vec2 hiddenSize{ 160, 0 };
    float maxCornerRadius = 28; // bottom corners: half the height, capped, so big shapes aren't blobs
    float flareRadius = 8;      // top corners curve outward into the screen edge (decision 025); 0 = square

    // Content layout inside the pill (PS §5.1 slots). Fonts are in pixels.
    // Each mode has one padding, used on every side; rounded content inside
    // gets the nested radius (pill radius - padding), so corners stay concentric.
    float compactInset = 4;     // compact padding
    float alertPadding = 8;
    float expandedPadding = 12;
    float expandedArtSize = 68; // expanded leading square: as tall as title + artist + caption beside it
    float slotGap = 10;         // between leading and center/bottom slots
    float rowSpacing = 4;
    float itemSpacing = 8;
    float looseItemSpacing = 24; // Row::loose, e.g. media controls
    float titleFontSize = 17;   // the type ramp steps 2 px: caption 13, body 15, title 17 (decision 024)
    float bodyFontSize = 15;
    float captionFontSize = 13; // the smallest text: ~11 px on screen at 100% after islandScale
    float displayFontSize = 34; // TextStyle::Display: a timer as the card's centre
    // Font weights, 100..900 (400 regular, 600 semibold; decision 024). Segoe UI Variable
    // (Windows 11) takes any value in between; other fonts use the nearest weight they have.
    float titleFontWeight = 600;
    float bodyFontWeight = 500;   // a touch heavier than regular: light text on a dark island reads thin
    float captionFontWeight = 500;
    float displayFontWeight = 350; // big and light, like a clock face
    // Darkens antialiased text strokes (DirectWrite's grayscale enhanced
    // contrast): 0 = none, 1 = Windows' usual, higher = bolder-looking edges.
    float textContrast = 1.5f;
    float inputFontSize = 15;   // a TextInput's text and placeholder: noticeable even when it isn't focused
    float iconSize = 18;
    float buttonSize = 36;
    float smallButtonSize = 24;   // Button::compact: list rows (a to-do's checkbox)
    float revealGlyphSize = 14;   // Button::revealOnHover (a to-do's delete): a smaller glyph, the same button and hover shape
    float inputHeight = 30;       // a TextInput: a little taller than the rows, easy to click into
    float listItemPadding = 4;    // Row::listItem: above and below its content, so list items stand apart
    float listItemInset = 6;      // Row::listItem: its content in from the hover background's edges
    float buttonLabelPadding = 10; // a text button: its label plus this on each side        // hover circle; the click area also takes in half the gaps around it
    float buttonGlyphSize = 16;
    float buttonPressScale = 0.85f;
    float progressHeight = 4;
    float progressHitHeight = 16; // taller than drawn, so it's easy to click
    float progressKnobSize = 12;  // the scrub handle shown on hover
    float timeLabelWidth = 56;  // fits "-1:02:05" at captionFontSize
    float ringThickness = 3;      // progress ring stroke (decision 010)
    float stepDotSize = 9;        // Steps: one dot per step
    float stepDotGap = 8;

    // Hover on buttons and the progress bar (decision 002: content only, not the pill).
    Ms hoverFadeDuration{ 120 };
    float artHoverGlyphSize = 22; // clickable art (media: opens its app): the "open" glyph shown on hover

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

    // Moving the island along the top edge (decision 026): press and hold to
    // lift it, drag sideways, let go. Snap distances are from where it's let go.
    Ms liftHold{ 400 };           // a press held this long without moving lifts the island
    float liftDrop = 6;           // lifted, it floats this far below the screen edge
    float centreSnap = 48;        // let go this close to the screen's centre: back to dead centre
    float edgeSnap = 24;          // ...this close to a side edge: flush with it (never less than flareRadius)

    // The island's shadow, under the pill and the bubble (decision 018); colour in shadowColor.
    float shadowBlur = 16;
    Vec2 shadowOffset{ 0, 4 };    // down: the island hangs from the top edge

    // Expanded's tabs: one per activity along the bottom of the card (decision 007).
    float tabHeight = 28;
    float tabGap = 4;
    float tabMaxWidth = 140;      // each tab's width; the group is centred, and shrinks only when they don't fit
    float homeButtonWidth = 44;   // Home's icon-only button, apart at the left (decision 009)

    // Home: a grid of panel tiles in the Expanded card (decision 009).
    std::size_t homeColumns = 4;  // columns' worth per Home row; a widget takes one, or two (Panel::wide); rows stretch to full width
    float tileHeight = 60;        // fits a title and a caption line inside tilePadding
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
    // A new track waits at most this long for its cover before showing without
    // it (a thumbnail that never loads must not hide the track).
    Ms mediaArtWait{ 1000 };
    // After a track change, the cover is read again this much later: apps
    // (Chrome) can send the title first and the right picture after.
    Ms mediaArtRecheck{ 800 };
    // A session read that fails (an app mid-switch) is retried this many times.
    std::size_t mediaReadRetries = 3;

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
    Color artHoverColor{ 0, 0, 0, 115 };           // dims clickable art under the pointer, behind its "open" glyph
    Color artHoverGlyphColor{ 255, 255, 255, 242 }; // that glyph: over dimmed art, so light in every theme (decision 022)
    Color trackColor{ 255, 255, 255, 61 };         // a progress bar's or ring's empty part
    Color liveColor{ 255, 255, 255, 230 };         // progress fill, waveform: when the content has no accent
    Color knobColor{ 255, 255, 255 };
    Color stepDoneColor{ 255, 255, 255, 191 };
    Color stepTodoColor{ 255, 255, 255, 51 };
    Color tabSelectedColor{ 255, 255, 255, 41 };
    Color hoverColor{ 255, 255, 255, 20 };         // a tab or tile under the pointer
    Color tileColor{ 255, 255, 255, 20 };          // Home's tile backgrounds
    Color inputColor{ 255, 255, 255, 26 };         // a text field's background (decision 016)
    Color placeholderTextColor{ 255, 255, 255, 128 }; // "Add a to-do…": like body text, a little dimmer
    Color dangerColor{ 255, 69, 58 };              // a destructive button's label: a dialog's "Clear all" (decision 020)
    Color imagePlaceholderColor{ 255, 255, 255, 31 }; // art that hasn't arrived
    Color shadowColor{ 0, 0, 0, 115 };             // under the island in every mode; its alpha is the shadow's strength
    Color pomodoroFocusColor{ 255, 99, 71 };       // accents (decision 010)
    Color pomodoroBreakColor{ 52, 199, 89 };

    // Pomodoro (decision 009): focus rounds with short breaks, a long break after the last round.
    Ms pomodoroFocus{ 1500000 };
    Ms pomodoroBreak{ 300000 };
    Ms pomodoroLongBreak{ 900000 };
    std::size_t pomodoroRounds = 4;
    // Lists (Row::list, decision 029): the window shows this many rows (the
    // half says there's more), the wheel moves scrollStep per notch, easing
    // over scrollDuration; a press dragged dragStartDistance picks a row up.
    float listVisibleRows = 4.5f;
    float scrollStep = 40;
    Ms scrollDuration{ 180 };

    // The Settings card's "Copy logs" button: how long it says "Copied" after a click.
    Ms logsNote{ 2000 };

    // The app themes the Settings card offers, in its order; the first is the
    // default (decision 022). tokens() already has the picked one applied.
    std::vector<Theme> themes;

    bool operator==(const Tokens&) const = default;
};

// The active tokens: defaults until the app applies a loaded tokens.json.
const Tokens& tokens();

// App only, on the UI thread. The one piece of global state in core/: config
// that everything reads and only the tokens.json reload writes.
void setTokens(const Tokens& tokens);

} // namespace cup
