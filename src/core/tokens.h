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
    // Derived when parsed, from the colours it ends up with (decision 030):
    Color island{ 0, 0, 0 }; // its islandColor
    Color accent;            // its liveColor (progress fills, the waveform)
    bool light = false;      // its island is pale: Settings lists dark and light apart
    bool operator==(const Theme&) const = default;
};

// How fast colours fade (decision 053): the user's pick in Settings.
enum class FadeSpeed { Fast, Medium, Slow };

struct Tokens {
    // Render layer: as wide as the island's monitor, so the island can move
    // along its top edge (decision 026), and this tall (TR §5, §9.1). The
    // width here is only a minimum, and the width layout() uses until the
    // platform says (tests).
    Vec2 canvas{ 720, 720 }; // room for the largest pill (the search card, a Centred card at its tallest, with tabs) and its shadow

    // One knob for the island's overall size: every length below is
    // multiplied by it, on top of the display's scaling (decision 017).
    // 1 = the sizes as written; below 1 = more compact.
    float islandScale = 0.85f;

    // Pill size per mode (PS §3.1). Height 0 = tucked into the screen edge.
    Vec2 idleSize{ 160, 28 };
    Vec2 compactSize{ 260, 32 };
    Vec2 alertSize{ 320, 64 };
    Vec2 expandedSize{ 480, 171 }; // every card and Home: the width a to-do's text needs (decisions 043, 047)
    Vec2 receptiveSize{ 380, 84 };
    Vec2 hiddenSize{ 160, 0 };
    float maxCornerRadius = 34; // bottom corners: half the height, capped, so big shapes aren't blobs
    float flareRadius = 8;      // top corners curve outward into the screen edge (decision 025); 0 = square

    // Content layout inside the pill (PS §5.1 slots). Fonts are in pixels.
    // Each mode has one padding, used on every side; rounded content inside
    // gets the nested radius (pill radius - padding), so corners stay concentric.
    float compactInset = 4;     // compact padding
    float alertPadding = 8;
    float expandedPadding = 18;
    float narrowRowWidth = 300;   // Row::narrow: at most this wide, centred (decision 070)
    float listCardInset = 8;      // CardLayout::List: at least this much more on the left and right (decision 030)
    float listCardWidth = 440;    // ...and its rows no wider than this, centred (a full-width card's, at the shipped sizes)
    float listCardVerticalInset = 6;  // ...this much more above and below its rows
    float listCardVisibleRows = 9.5f; // ...and a page that is one list (Settings' root) scrolls past this many rows (decision 037)
    float listCardMinHeight = 240;    // ...and never shorter than this: room to breathe (decision 030)
    // CardLayout::Search (decision 079): wider than other cards, as tall as
    // its rows (just the field when nothing is typed), and a bigger field.
    float searchCardWidth = 640;
    float wideCardWidth = 640; // a card that asks for more room (Content::wideCard, decision 085): the timer
    float searchInputHeight = 48;
    float searchInputFontSize = 19;
    float resultIconSize = 32;           // a search result's icon (Button::picture)
    float resultFontSize = 17;           // TextStyle::Result: a search result's name
    float resultDetailFontSize = 14;     // TextStyle::ResultDetail: where it is, under it
    // Floating cards (decision 081): a card apart from the island, in its look.
    // Its provider picks a width; it's as tall as its content, up to floatMaxHeight.
    float floatNarrowWidth = 360;
    float floatMediumWidth = 480;
    float floatWideWidth = 640;
    float floatMaxHeight = 640;
    Vec2 floatMinSize{ 320, 160 };  // a resizable one stops here
    float floatIslandGap = 16;      // FloatStart::NearIsland: this far under the pill
    float floatCascade = 28;        // opening where another one already is: this far right and down from it
    float floatCloseSize = 28;      // the ✕ at the right end of its title bar
    float floatTitleBarHeight = 34; // a strip along its top holding the ✕, like a window's title bar; it drags the card too
    float floatCornerRadius = 8;    // a window's corners, as Windows 11 rounds them: not the island's
    float floatResizeBorder = 6;    // a resizable card's edges: this wide a band inside them resizes it
    float expandedArtSize = 68; // expanded leading square: as tall as title + artist + caption beside it
    float slotGap = 10;         // between leading and center/bottom slots
    float rowSpacing = 4;
    float itemSpacing = 8;
    float looseItemSpacing = 24; // Row::loose, e.g. media controls
    float titleFontSize = 17;   // the type ramp steps 2 px: caption 13, body 15, title 17 (decision 024)
    float bodyFontSize = 15;
    float captionFontSize = 13; // the smallest text: ~11 px on screen at 100% after islandScale
    float displayFontSize = 48; // TextStyle::Display: a timer as the card's centre
    float headingFontSize = 14; // TextStyle::Heading: a group's name, a little over caption size
    // Font weights, 100..900 (400 regular, 600 semibold; decision 024). Segoe UI Variable
    // (Windows 11) takes any value in between; other fonts use the nearest weight they have.
    float titleFontWeight = 700;   // bold: Segoe UI Variable's heaviest
    float bodyFontWeight = 600;    // semibold: text on the island reads heavier than on a page (decision 024)
    float captionFontWeight = 600;
    float displayFontWeight = 300; // big and still the lightest, like a clock face
    float headingFontWeight = 700; // a heading stands out from the rows under it: bold, and the title colour
    // Darkens antialiased text strokes (DirectWrite's grayscale enhanced
    // contrast): 0 = none, 1 = Windows' usual, higher = bolder-looking edges.
    float textContrast = 1.5f;
    float inputFontSize = 15;   // a TextInput's text and placeholder: noticeable even when it isn't focused
    float iconSize = 18;
    float buttonSize = 36;
    float primaryButtonSize = 56;  // Button::primary: the card's main action, a filled circle (decision 064)
    float primaryGlyphSize = 22;
    float playGlyphNudge = 0.08f;  // a primary ▶ moved right by this share of its width: a triangle's optical centre (decision 071)
    Vec2 toastPadding{ 14, 6 };    // a toast: room around its text and button inside its pill (decision 071)
    // A row's menu (decision 086): drawn in islandColor, its lit item in buttonHoverColor, its lines in trackColor.
    float menuMinWidth = 200;      // ...at least this wide, at most menuMaxWidth: as its longest label and shortcut
    float menuMaxWidth = 340;
    float menuItemHeight = 34;
    float menuPadding = 6;         // ...room around its items, inside it
    float menuItemInset = 12;      // ...an item's label and shortcut this far in from its lit band's ends
    float menuRadius = 14;
    float menuGap = 6;             // ...opened by the keys: this far beside its row
    float menuLineGap = 9;         // ...the room a line between groups takes
    float dividerThickness = 1;    // a Divider's line
    float dialInset = 10;          // a Dial: room between its time (and label) and its ring (decision 065)
    float groupGap = 20;           // Row::apart: space above a card's group (decision 066)
    float primaryHoverOpacity = 0.8f; // Button::primary under the pointer: its fill this much as strong (decision 066)
    float smallButtonSize = 32;   // Button::compact: list rows (a to-do's checkbox)
    float revealGlyphSize = 14;   // Button::revealOnHover (a to-do's delete): a smaller glyph, the same button and hover shape
    float inputHeight = 36;       // a TextInput: a little taller than the rows, easy to click into
    float listItemPadding = 6;    // Row::listItem: above and below its content; = listItemInset, so a checkbox's corners nest in the row's (decision 006)
    float listItemInset = 6;      // Row::listItem: its content in from the hover background's edges
    float sectionGap = 12;        // Row::section: space above a group's heading (decision 037)
    float listItemTextInset = 14; // ...where the edge item is text: clear of the background's rounded end (decision 030)
    float editInset = 6;          // editable Text is drawn this far in from its stretch, which its field fills when edited (decision 043)
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
    float badgeSize = 8;          // a notification dot (decision 038)
    float badgeInset = 7;         // ...at least this far in from a widget's or tab's edges (its corner's rounding sets it, decision 038)
    float swatchSize = 24;        // Swatch: a colour sample (a theme on Settings' list, decision 030)
    Vec2 switchSize{ 36, 20 };    // Switch: an on/off setting's track (decision 053); the knob is its height, inset
    float switchKnobInset = 3;    // ...the knob this far inside the track
    float tagHeight = 18;         // Tag: "New" beside a setting not seen yet (decision 048)
    float tagPadding = 7;         // ...its text this far in from the pill's ends

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
    float bubbleGap = 8;          // pill to bubble at rest without flares; with them, the gap is both flares (decision 025)
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

    // Glass (decision 089): the blurred desktop behind the island, at one
    // brightness (glassLuminosityColor) like Windows' own acrylic, the island's
    // colours over it, and a light rim inside its edge, brightest along the
    // bottom. Windows 11 and dark islands only; colours in glassLuminosityColor,
    // glassRimColor, glassShadowColor.
    bool glass = false;           // drawn as glass now (withGlass): set by the app, not tokens.json
    float glassOpacity = 0.15f;   // how strong the island's colours are over the blur: 1 solid, 0 none (acrylic's tint opacity)
    float glassPlainOpacity = 0.7f; // ...over the plain blur, where Windows won't even out its brightness: it needs more
    // Glass with the cover's colours (decision 090): the island stays
    // Midnight, and the cover's colour glows up from its bottom edge.
    float glassGlowHeight = 0.3f;   // the glow's reach: this share of the island's height, fading out to its top
    float glassGlowOpacity = 0.85f; // how strong the glow is along the bottom edge
    float glassRimWidth = 1;      // the rim's width, inside the edge

    // Expanded's tabs: one per activity along the top of the card (decisions 007, 047).
    float tabHeight = 36;
    float tabGap = 4;
    float tabMaxWidth = 140;      // each tab's width; the group is centred, and shrinks only when they don't fit
    float homeButtonWidth = 44;   // Home's icon-only button, apart at the left (decision 009)
    float tabMutedOpacity = 0.5f; // a tab not shown, Home and the pin included: its content grey and this strong until hovered (decision 072)

    // Home: a grid of panel tiles in the Expanded card (decision 009).
    std::size_t homeColumns = 4;  // columns' worth per Home row; a widget takes one, or two (Panel::wide); rows stretch to full width
    float tileHeight = 60;        // fits a title and a caption line inside tilePadding
    float tileGap = 8;
    float tilePadding = 8;        // inside a tile, on every side
    float focusRingWidth = 2;     // the keyboard's focus ring (decision 033), drawn inside what has focus

    // Smaller transitions: the ears and corners folding, a lift sinking, a
    // list's rows (ease-out, TR §5). The pill's own morph is a spring, below.
    Ms morphDuration{ 300 };
    Bezier morphEase{ { 0.16f, 1.0f }, { 0.30f, 1.0f } };
    // The pill's morph (decision 080, core/motion.h): a spring, settled by the
    // end of its time. Bounce 0 never goes past; 0.25 goes about 3% past and
    // back. Growing overshoots a little, shrinking doesn't. Its time grows with
    // the square root of how far it goes: the shortest for none, the reference
    // at morphReferenceDistance, never more than the max (only a guard).
    Ms morphGrowShortest{ 220 };
    Ms morphGrowReference{ 400 };
    Ms morphGrowMax{ 600 };
    float morphGrowBounce = 0.25f;
    Ms morphShrinkShortest{ 180 };
    Ms morphShrinkReference{ 320 };
    Ms morphShrinkMax{ 480 };
    float morphShrinkBounce = 0.0f;
    float morphReferenceDistance = 500;
    float morphSpringSettle = 7; // how front-loaded the spring is: 8.5 felt rushed on big morphs, 6 slow on all
    // Small things settling with a hint of overshoot: the tab marker gliding,
    // a toast coming up, a switch's knob, content arriving.
    Bezier settleEase{ { 0.3f, 1.3f }, { 0.5f, 1.0f } };
    float contentArriveScale = 0.96f; // new content grows from this while fading in
    Ms contentArriveDuration{ 200 };  // ...over this, settling with the pill, not after it
    Ms contentStagger{ 14 };          // ...each row this long after the one above
    std::size_t contentStaggerRows = 4; // rows past this arrive with the last
    // A new alert in a pill that keeps its size: it swells this much and springs back.
    float alertPulseScale = 1.05f;
    Ms alertPulseDuration{ 420 };
    // Let go mid-drag, the island or bubble carries the pointer's speed into its landing.
    float flingMaxSpeed = 2;  // pixels per ms
    Ms flingStale{ 80 };      // the pointer still this long before letting go: no fling
    // Windows' "Animation effects" off (decision 080): morphs are this short, with
    // no overshoot, stagger, fling or pulse. Set by the app, not by tokens.json.
    bool reducedMotion = false;
    Ms reducedMotionDuration{ 120 };

    // Content crossfades slightly after the pill starts morphing.
    Ms contentFadeDuration{ 150 };
    // New content starts arriving once the growing pill is this share of the
    // way to its new size (decision 080): with the spring that's early, and it
    // follows the morph's own timing. A shrinking pill shows it at once.
    float contentRevealAt = 0.6f;
    // Colours easing to new ones (a cover's tint, a theme pick; decision 051):
    // the island's fill, the shadow, marks and content shapes. Text switches
    // at once. One duration per speed the user can pick in Settings (decision 053).
    Ms recolorFast{ 800 };
    Ms recolorMedium{ 2000 };
    Ms recolorSlow{ 4000 };
    FadeSpeed fadeSpeed = FadeSpeed::Medium; // the user's pick: set by the app from Settings, not by tokens.json

    // Press feedback: squash on mouse-down, spring back with overshoot (y > 1) on release.
    float pressScale = 0.96f;
    Ms pressDownDuration{ 100 };
    Bezier pressDownEase{ { 0.4f, 0.0f }, { 0.2f, 1.0f } };
    Ms pressReleaseDuration{ 220 };
    Bezier pressReleaseEase{ { 0.34f, 1.56f }, { 0.64f, 1.0f } };

    // Arbitration (PS §6).
    Ms alertDuration{ 3000 };
    Ms toastDuration{ 5000 }; // a toast at a card's bottom goes after this (decision 071)
    Ms toastSlideDuration{ 260 }; // ...and slides up from below the card's edge, fading in, and back down, over this
    Ms menuOpenDuration{ 140 };   // a row's menu grows and fades in from where it opens, over this (decision 086); it goes at once
    Ms floatHandOff{ 34 }; // a floating card's flow (decision 081): its layer stays this long after the card's window shows, so nothing blinks

    Ms expandedIdleTimeout{ 8000 };
    Ms heldAlertMaxAge{ 30000 };
    std::size_t alertQueueCapacity = 5;

    // Media (PS §10): a paused track stays on the island this long, then ends.
    Ms mediaPausedLinger{ 300000 };
    // After any app starts/stops playing, ask Windows again this much later:
    // its "current session" can switch slightly after the playback event.
    Ms mediaSessionRecheck{ 300 };
    // With no session left, what's shown stays this long before it ends:
    // some apps drop theirs for a moment on every track change (decision 074).
    Ms mediaSessionGoneGrace{ 1500 };
    // A track shows at once; its cover fades in over this when it's read (decision 056).
    Ms mediaArtFade{ 400 };
    // After a track change, the cover is read again this much later: apps
    // (Chrome) can send the title first and the right picture after.
    Ms mediaArtRecheck{ 800 };
    // A track still without a cover is read again this many times, each wait
    // one mediaArtRecheck longer (0.8, 1.6, 2.4 s...): Spotify can add the
    // picture seconds after the title, with no event, or hand out one that
    // can't be read yet (decision 056).
    std::size_t mediaArtRechecks = 5;
    // Reads of a session go one at a time (decision 005); one still running
    // after this is taken as stuck, and doesn't hold the next one back.
    Ms mediaReadStuck{ 2000 };
    // Hidden for a fullscreen app, the island checks again this often: Windows'
    // exclusive-fullscreen flag can clear without any window event (decision 059).
    Ms fullscreenRecheck{ 2000 };
    // A session read that fails (an app mid-switch) is retried this many times.
    std::size_t mediaReadRetries = 3;

    // The island in the playing cover's colours (decision 051): the cover's
    // hue at these OKLCH lightnesses (0..1) and chromas, in the theme's own
    // recipe (decision 022): a dark theme gets a tinted near-black, a light one a pale tint.
    Ms coverTintLinger{ 1500 };          // nothing plays any more: the cover's colours stay this long (a track change pauses for a moment)
    float coverVividSaturation = 0.25f;  // a cover's pixel counts as coloured from this saturation (0..1) up
    float coverMinVivid = 0.05f;         // ...and a cover needs this share of them, or it's grey and the theme stays
    float coverDarkIsland = 0.24f;       // dark themes: the island's lightness
    float coverDarkAccent = 0.72f;       // ...the accent's (progress, waveform, tiles, hover)
    float coverDarkInk = 0.94f;          // ...text and icons'
    float coverLightIsland = 0.92f;      // light themes: the same three
    float coverLightAccent = 0.58f;
    float coverLightInk = 0.3f;
    // How coloured the island and the accent are (OKLCH chroma, 0: grey), from
    // a muted cover to a vivid one (decision 054): [muted, vivid].
    Vec2 coverIslandChroma{ 0.04f, 0.12f };
    Vec2 coverAccentChroma{ 0.12f, 0.22f };
    float coverFullChroma = 0.2f;        // a cover's main colour this vivid (chroma) or more counts as fully vivid
    float coverSecondShare = 0.3f;       // a second hue needs this share of the main one's weight to make a gradient
    float coverInkChroma = 0.03f;        // ...text and icons: barely, so they read as white or ink
    float coverDarkGlow = 0.45f;         // glass (decision 090): the glow's lightness; its chroma is the accent's

    // Colours: every colour the island draws. tokens.json writes [r, g, b] or
    // [r, g, b, alpha], channels 0..255 and alpha 0..1, like CSS rgba().
    Color islandColor{ 0, 0, 0 };
    Color islandHighlightColor{ 46, 46, 46 }; // the pill while the dragged bubble is over it (decision 007)
    // The island's right end: a gradient from islandColor (decision 054). Not
    // in tokens.json: islandColor's, unless a two-colour cover sets it.
    Color islandSecondColor{ 0, 0, 0 };
    // The glow along the island's bottom edge (decision 090): glass with the
    // cover's colours. Not in tokens.json: none (clear) unless a cover sets it.
    Color islandGlowColor{ 0, 0, 0, 0 };
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
    Color tileColor{ 255, 255, 255, 20 };          // Home's tile backgrounds, always shown (tabs have none, decision 072)
    Color focusRingColor{ 255, 255, 255, 153 };    // around what the keyboard has focus on (decision 033): a theme's accent
    Color inputColor{ 255, 255, 255, 26 };         // a text field's background (decision 016)
    Color placeholderTextColor{ 255, 255, 255, 128 }; // "Add a to-do…": like body text, a little dimmer
    Color tagTextColor{ 255, 255, 255 };           // a Tag's text; its pill is badgeColor, like the dots it goes with (decision 048)
    Color selectionColor{ 10, 132, 255, 115 };     // selected text in a field (decision 043): readable on dark and light themes
    Color dangerColor{ 255, 69, 58 };              // a destructive button's label: a dialog's "Clear all" (decision 020)
    Color badgeColor{ 255, 69, 58 };               // a notification dot (decision 038): the red every OS uses for "new"
    Color imagePlaceholderColor{ 255, 255, 255, 31 }; // art that hasn't arrived
    Color shadowColor{ 0, 0, 0, 115 };             // under the island in every mode; its alpha is the shadow's strength
    Color glassShadowColor{ 0, 0, 0, 51 };         // ...as glass (decision 089): fainter, since it also shows through the island
    Color glassRimColor{ 255, 255, 255, 46 };      // glass's rim (decision 089), at its brightest along the bottom
    // Glass's brightness (decision 089): the blur takes this colour's
    // lightness, keeping the desktop's hue, this strongly (alpha), as Windows'
    // acrylic does; 0 alpha: the desktop's own brightness shows, lighter.
    Color glassLuminosityColor{ 44, 44, 44, 245 };
    Color pomodoroFocusColor{ 255, 99, 71 };       // accents (decision 010)
    Color pomodoroBreakColor{ 52, 199, 89 };

    // Pomodoro (decision 009): focus rounds with short breaks, a long break after the last round.
    Ms pomodoroFocus{ 1500000 };
    Ms pomodoroBreak{ 300000 };
    // Setting the plan on the dormant card (decision 044): each − / + moves by a step, from one step up to the max.
    Ms pomodoroFocusStep{ 300000 };
    Ms pomodoroFocusMax{ 7200000 };
    Ms pomodoroBreakStep{ 60000 };
    Ms pomodoroBreakMax{ 3600000 };
    Ms pomodoroLongBreak{ 900000 };
    std::size_t pomodoroRounds = 4;
    // A session counts as finished (decision 061) with at most this many focus phases skipped.
    std::size_t pomodoroFocusSkipsAllowed = 1;
    std::size_t todoMaxLength = 150; // characters in a to-do: wraps onto a few lines at most (decision 045)
    // Lists (Row::list, decision 029): the window shows this many rows (the
    // half says there's more), the wheel moves scrollStep per notch, easing
    // over scrollDuration; a press dragged dragStartDistance picks a row up.
    float listVisibleRows = 4.5f;
    float scrollStep = 40;
    Ms scrollDuration{ 180 };

    // The Settings card's "Copy logs" button: how long it says "Copied" after a click.
    Ms logsNote{ 2000 };

    // Updates (decision 036): the first check this long after start (not in
    // the middle of starting up), then one per interval, kept across restarts;
    // a check that failed (offline) tries again after updateRetry.
    Ms updateFirstCheckDelay{ 15000 };
    Ms updateCheckInterval{ 86400000 };
    Ms updateRetry{ 3600000 };
    // File search (decision 076): this long after start, the app looks for
    // the service, sets it up if missing, and asks it to update if older.
    Ms searchSetupDelay{ 20000 };
    // An older service asked to update is looked at again this long after
    // (just past the service's own 10 min between updates); still old, it's
    // asked again, at most serviceUpdateTries times per start (decision 076).
    Ms serviceUpdateRetry{ 660000 };
    std::size_t serviceUpdateTries = 3;
    // A double-tap shortcut ("Double Ctrl", decision 079): the second press
    // within doubleTap of the first letting go; each press at most
    // doubleTapHold long (a held Ctrl is someone about to press Ctrl+C).
    Ms doubleTap{ 350 };
    Ms doubleTapHold{ 300 };
    // Search's card (decision 079): at most this many rows of each group, so
    // a hundred files never push the app you meant off the list; the list's
    // window in rows; and how old the list of apps may get before the card
    // reads it again (decision 077).
    std::size_t searchAppRows = 5;
    std::size_t searchSettingRows = 3;
    std::size_t searchFileRows = 8;
    std::size_t searchIconCache = 400; // result icons kept in memory; past it, they're all read again
    float searchVisibleRows = 8.5f;
    Ms launcherRefresh{ 600000 };

    // The app themes the Settings card offers, in its order; the first is the
    // default (decision 022). tokens() already has the picked one applied.
    std::vector<Theme> themes;

    bool operator==(const Tokens&) const = default;
};

// The colour fade's duration at the picked speed (recolorFast/Medium/Slow).
Ms recolorDuration(const Tokens& t);

// The island's fill as drawn (decision 089): `color`, or as glass, see-through
// by glassOpacity; by glassPlainOpacity over the plain blur (`evenBlur`
// false: Windows refused to even out its brightness). The pill, the bubble
// and the neck between them all use it.
Color islandFill(const Tokens& t, Color color, bool evenBlur = true);

// A pale colour: perceived brightness past half. A theme whose island is pale
// is a light theme (decision 030).
bool pale(Color color);

// `t` as glass if the user picked it (decision 089), except on a pale island
// (a light theme, or its cover tint): glass there turns its dark text grey on
// whatever the desktop shows.
Tokens withGlass(Tokens t, bool picked);

// The active tokens: defaults until the app applies a loaded tokens.json.
const Tokens& tokens();

// App only, on the UI thread. The one piece of global state in core/: config
// that everything reads and only the tokens.json reload writes.
void setTokens(const Tokens& tokens);

} // namespace cup
