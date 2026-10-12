#pragma once
// State -> what the renderer should show: pill geometry plus every content
// item with its frame. Pure; the renderer applies a Scene and decides nothing.

#include "core/geometry.h"
#include "core/island.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cup {

struct Placed {
    std::string key;    // "<slot>/<row>/<item>": stable across updates, so the renderer can diff
    Item item;          // TimeLabels arrive already formatted as Text
    Rect frame;         // pill coordinates
    Rect hitFrame;      // clickable area: the frame plus half the gap to each neighbour, so there are no dead strips
    float cornerRadius = 0; // nested in the pill's corner (pill radius - padding), capped at half the frame
    std::optional<Color> accent; // the content's accent, for the renderer to tint live data (decision 010)
    std::string scrollAction;    // its row's (Row::scrollAction): the wheel over it sends this
    std::optional<Rect> listRow; // its row is a list item (Row::listItem): lit here while the pointer is on the row
    float listRowRadius = 0;
    std::string providerId;
    std::string activityId;
    std::optional<Timeline> ticking; // time labels: the text changes as playback moves
    bool operator==(const Placed&) const = default;
};

// The Split bubble (PS §3.1): the 2nd activity's `minimal` slot, in a
// shape like the pill's.
struct BubbleScene {
    Rect frame;                    // at rest, render-layer coordinates
    std::vector<Placed> elements;  // bubble coordinates
    std::string activityId;
    std::optional<Vec2> dragCentre; // being dragged: centred on the cursor here, no easing (decision 007)
};

// Expanded with several activities, or activities and panels: a tab per
// activity along the bottom of the card (decision 007), and a square Home
// button apart at the left when there are panels (decision 009). Clicking a
// tab switches to it (ActivitySwitched, or HomeOpened for Home).
struct Tab {
    Rect frame;     // pill coordinates: drawn highlight
    Rect hitFrame;  // clickable area: the frame, or more (Home's reaches into the card's corner)
    std::string id; // the activity's id; empty = Home
};

struct TabStrip {
    std::vector<Tab> tabs;  // Home's button, then activities in start order: stable, so a click doesn't reorder them
    std::optional<std::size_t> active; // the tab of what's shown; none for a dormant panel's card
    float radius = 0;       // tab highlight corners, nested in the card's
};

// A panel's widget on Home (decisions 009, 019). A click on it opens its
// card (TileClicked) if it has one, or sends its tap action.
struct Tile {
    Rect frame;     // pill coordinates; the renderer draws a faint background here
    std::string id; // the panel's (and its activity's) id
    bool opens = false;     // has a card: a click opens it (TileClicked)
    std::string providerId;
    std::string tapAction;  // a click sends this action instead (Panel::tapAction)
    bool clickable() const { return opens || !tapAction.empty(); } // lights up under the pointer
};

struct Scene {
    Mode mode = Mode::Idle;
    Vec2 size;                     // pill target size, render-layer coordinates
    Vec2 offset;                   // pill target top-left: on the top edge where the island rests (decision 026)
    bool lifted = false;           // held and dragged: floats liftDrop below the edge, all corners round, no flares
    bool flushLeft = false;        // the pill touches the screen's left edge: no flare on that side (decision 026)
    bool flushRight = false;       // the rightmost shape (the bubble in Split) touches the right edge
    std::vector<Placed> elements;  // content of the activity/alert being shown
    std::string activityId;        // whose content the pill shows (empty: none, or a standalone alert)
    std::optional<BubbleScene> bubble;
    std::optional<TabStrip> tabs;  // Expanded with several activities, or activities and panels
    std::vector<Tile> tiles;       // Home
    float tileRadius = 0;          // tile background corners, nested in the card's
    bool dropTarget = false;       // the dragged bubble is over the pill: letting go swaps them (pill lightens)
    Vec2 bubbleTuck;               // no bubble: it shrinks to this point, on the top edge inside the pill
    std::string label;             // accessible name of what's shown
    Ms now{};                      // when the scene was laid out (progress bars animate from here)
    std::optional<Ms> clockTick;   // Idle shows the time: re-layout at this moment (the next minute)
};

// A Text's width in pixels on one line, as it will be drawn. The platform
// measures with the real font (DirectWrite); core can't, it has no fonts.
using MeasureText = std::function<float(const Text& text)>;

// `measure` is only needed to centre content of unknown width (Expanded's
// tabs). Without it, a rough per-character estimate is used (tests).
Scene layout(const IslandState& state, Ms now, const MeasureText& measure = {});

// What the interaction layer must cover: the pill, plus the bubble at rest in Split.
Rect interactiveBounds(const Scene& scene);

// The pill's bottom corner radius at `size`: half the height, capped by
// tokens().maxCornerRadius. The renderer draws the same curve.
float pillCornerRadius(Vec2 size);

// The Home tile under `point` (pill coordinates), if any.
const Tile* tileAt(const Scene& scene, Vec2 point);

// When the scene next changes without any event (a time label's second
// ticking over, the idle clock's minute), so the app can arm its timer. Nothing = static.
std::optional<Ms> sceneDeadline(const Scene& scene);

} // namespace cup
