#pragma once
// State -> what the renderer should show: pill geometry plus every content
// item with its frame. Pure; the renderer applies a Scene and decides nothing.

#include "core/geometry.h"
#include "core/island.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// Action ids of the host's own Expanded switcher (no provider id).
inline constexpr const char* kIslandNext = "island.next";
inline constexpr const char* kIslandPromote = "island.promote";

struct Placed {
    std::string key;    // "<slot>/<row>/<item>": stable across updates, so the renderer can diff
    Item item;          // TimeLabels arrive already formatted as Text
    Rect frame;         // pill coordinates
    Rect hitFrame;      // clickable area: the frame plus half the gap to each neighbour, so there are no dead strips
    float cornerRadius = 0; // nested in the pill's corner (pill radius - padding), capped at half the frame
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

struct Scene {
    Mode mode = Mode::Idle;
    Vec2 size;                     // pill target size, render-layer coordinates
    Vec2 offset;                   // pill target top-left: centered, flush with the top edge
    std::vector<Placed> elements;  // content of the activity/alert being shown
    std::string activityId;        // whose content the pill shows (empty: none, or a standalone alert)
    std::optional<BubbleScene> bubble;
    bool dropTarget = false;       // the dragged bubble is over the pill: letting go swaps them (pill lightens)
    Vec2 bubbleTuck;               // no bubble: it shrinks to this point inside the pill
    std::string label;             // accessible name of what's shown
    Ms now{};                      // when the scene was laid out (progress bars animate from here)
};

Scene layout(const IslandState& state, Ms now);

// What the interaction layer must cover: the pill, plus the bubble at rest in Split.
Rect interactiveBounds(const Scene& scene);

// The pill's bottom corner radius at `size`: half the height, capped by
// tokens().maxCornerRadius. The renderer draws the same curve.
float pillCornerRadius(Vec2 size);

// When the scene next changes without any event (a time label's second
// ticking over), so the app can arm its timer. Nothing = static.
std::optional<Ms> sceneDeadline(const Scene& scene);

} // namespace cup
