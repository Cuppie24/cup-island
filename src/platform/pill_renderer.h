#pragma once
// Draws the island: the pill, the Split bubble beside it (which becomes
// Expanded's side card), and the liquid neck between them. apply() sets targets and implicit animations do the
// morph (TR §5), so this never redraws per frame.

#include "core/layout.h"
#include "platform/blob.h"
#include "platform/graphics_device.h"
#include "platform/liquid_bridge.h"
#include "platform/win.h"

#include <optional>
#include <string>

namespace cup::platform {

class PillRenderer {
public:
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& root, const Scene& initial);
    void apply(const Scene& scene);

    // Rebuilds the implicit animations from the current tokens() (after a reload).
    void refreshAnimations();

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown(bool bubble) { (bubble ? bubble_ : pill_).pressDown(); }
    void pressRelease(bool bubble) { (bubble ? bubble_ : pill_).pressRelease(); }

    // Pointer feedback on one of the pill's content elements (see ElementVisuals).
    void setHovered(const std::string& key) { pill_.elements().setHovered(key); }
    void pressElement(const std::string& key) { pill_.elements().press(key); }
    void releaseElement(const std::string& key) { pill_.elements().release(key); }
    void scrubElement(const std::string& key, float fraction) { pill_.elements().scrub(key, fraction); }
    void endScrub(const std::string& key, std::optional<float> seekTo, Ms now) { pill_.elements().endScrub(key, seekTo, now); }

    // Expanded's side card: highlight the row under the pointer (side-card coordinates), or none.
    void setSideRowHovered(std::optional<Rect> row, float cornerRadius) { bubble_.setRowHighlight(row, cornerRadius); }

private:
    GraphicsDevice device_;
    Blob pill_;
    Blob bubble_;
    LiquidBridge bridge_;
};

} // namespace cup::platform
