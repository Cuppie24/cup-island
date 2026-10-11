#pragma once
// State -> what the renderer should show: pill geometry plus every content
// item with its frame. Pure; the renderer applies a Scene and decides nothing.

#include "core/geometry.h"
#include "core/island.h"

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
    std::string providerId;
    std::string activityId;
    std::optional<Timeline> ticking; // time labels: the text changes as playback moves
    bool operator==(const Placed&) const = default;
};

struct Scene {
    Mode mode = Mode::Idle;
    Vec2 size;                     // pill target size, render-layer coordinates
    Vec2 offset;                   // pill target top-left: centered, flush with the top edge
    std::vector<Placed> elements;  // content of the activity/alert being shown
    bool showBubble = false;       // Split: second activity's bubble (not rendered yet)
    std::string label;             // accessible name of what's shown
    Ms now{};                      // when the scene was laid out (progress bars animate from here)
};

Scene layout(const IslandState& state, Ms now);

// The pill's bottom corner radius at `size`: half the height, capped by
// tokens().maxCornerRadius. The renderer draws the same curve.
float pillCornerRadius(Vec2 size);

// When the scene next changes without any event (a time label's second
// ticking over), so the app can arm its timer. Nothing = static.
std::optional<Ms> sceneDeadline(const Scene& scene);

} // namespace cup
