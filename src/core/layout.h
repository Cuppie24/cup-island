#pragma once
// State -> what the renderer should animate toward. Pure; the renderer only
// applies a Scene and never decides anything itself.

#include "core/geometry.h"
#include "core/island.h"

namespace cup {

struct Scene {
    Mode mode = Mode::Idle;
    Vec2 size;                 // pill target size, render-layer coordinates
    Vec2 offset;               // pill target top-left: centered, flush with the top edge
    float detailOpacity = 0;   // expanded detail content (placeholder layout for now)
    bool showBubble = false;   // Split: second activity's bubble (not rendered yet)
    Content content;           // what the pill is about (not rendered yet: needs text)
};

Scene layout(const IslandState& state);

} // namespace cup
