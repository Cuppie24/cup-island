#pragma once
// Draws the island: the pill, the Split bubble and the liquid neck between
// them. apply() sets targets and implicit animations do the morph (TR §5),
// so this never redraws per frame.

#include "core/layout.h"
#include "platform/blob.h"
#include "platform/graphics_device.h"
#include "platform/liquid_bridge.h"
#include "platform/win.h"

namespace cup::platform {

class PillRenderer {
public:
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& root, const Scene& initial);
    void apply(const Scene& scene);

    // Rebuilds the implicit animations from the current tokens() (after a reload).
    void refreshAnimations();

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown(bool bubble) { (bubble ? bubbleBlob() : pillBlob()).pressDown(); }
    void pressRelease(bool bubble) { (bubble ? bubbleBlob() : pillBlob()).pressRelease(); }

    // Same, for one content element (a button) instead of the whole pill.
    void pressElement(const std::string& key) { pillBlob().pressElement(key); }
    void releaseElement(const std::string& key) { pillBlob().releaseElement(key); }

private:
    Blob& pillBlob() { return blobs_[pill_]; }
    Blob& bubbleBlob() { return blobs_[1 - pill_]; }

    GraphicsDevice device_;
    Blob blobs_[2];
    int pill_ = 0; // which blob plays the pill right now
    LiquidBridge bridge_;
};

} // namespace cup::platform
