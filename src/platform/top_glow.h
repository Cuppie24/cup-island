#pragma once
// The expanded island's top band, lit (decision 108): a soft glow fading down
// from the top edge while the pointer is on the band, where a click closes
// the island. The theme's text colour at collapseGlowOpacity, so it's a light
// glow on a dark island and a shade on a light one. It spreads in from the
// top edge's middle as it fades in (collapseGlowSpread, collapseGlowDuration)
// and shrinks back as it fades out.
// In the island's own shape: across the body and its flared top corners,
// cut by the outline's live snapshot (the shadow's and the glass's mask), so
// it follows the corners. Before Windows 10 1903 there's no snapshot: then
// it's the body's rectangle.

#include "platform/win.h"

namespace cup::platform {

class TopGlow {
public:
    // In `body`, under `content`; sized by expressions on the body (`pill`) and
    // its property set (`shape`). `outline`: the snapshot, or null.
    void build(wuc::Compositor const& compositor, wuc::ShapeVisual const& body, wuc::CompositionPropertySet const& shape,
               wuc::Visual const& content, wuc::CompositionVisualSurface const& outline);
    void show(bool on); // under the pointer, or not
    void recolor();     // tokens changed: the theme's text colour, faded to

private:
    wuc::Compositor compositor_{ nullptr };
    wuc::SpriteVisual visual_{ nullptr };
    wuc::CompositionColorGradientStop edge_{ nullptr }, clear_{ nullptr };
    bool on_ = false;
};

} // namespace cup::platform
