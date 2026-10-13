#pragma once
// The liquid neck between the pill and the bubble (PS §3.4: the bubble "buds
// off the pill's edge and separates, metaball-style"). A black strand from
// one shape's centre to the other's, thick when their edges touch, thinning
// to nothing at `bridgeReach` apart. Pure expressions over both shapes' live
// values, so it works mid-morph and mid-drag with no app code per frame.

#include "platform/win.h"

namespace cup::platform {

class LiquidBridge {
public:
    // Between two shapes' visuals: a Blob's body(), or the daydream's droplet (decision 050).
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent, wuc::Visual const& a, wuc::Visual const& b);
    void refreshAnimations(); // after a tokens.json reload
    void setEnabled(bool on); // off while the bubble is dragged

private:
    wuc::CompositionPropertySet params_{ nullptr }; // Thickness, Reach, On
    bool on_ = true;
    wuc::CompositionColorBrush brush_{ nullptr }; // the island's colour
    wuc::ShapeVisual visual_{ nullptr };          // as big as the render layer
};

} // namespace cup::platform
