#pragma once
// The expanded island's top band, lit (decision 108): a soft glow fading down
// from the top edge while the pointer is on the band, where a click closes
// the island. The theme's text colour at collapseGlowOpacity, so it's a light
// glow on a dark island and a shade on a light one. Under the content.

#include "core/geometry.h"
#include "platform/win.h"

#include <optional>

namespace cup::platform {

class TopGlow {
public:
    // Made on first use, at the bottom of `content` (pill coordinates), under what it shows.
    void show(wuc::Compositor const& compositor, wuc::ContainerVisual const& content, std::optional<Rect> band);
    void recolor(); // tokens changed: the theme's text colour, faded to

private:
    wuc::SpriteVisual visual_{ nullptr };
    wuc::CompositionColorGradientStop edge_{ nullptr }, clear_{ nullptr };
    std::optional<Rect> at_;
};

} // namespace cup::platform
