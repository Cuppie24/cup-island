#pragma once
// Draws the pill and its content. apply() sets targets and implicit
// animations do the morph (TR §5), so this never redraws per frame.

#include "core/layout.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

namespace cup::platform {

class PillRenderer {
public:
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& root, const Scene& initial);

    // Returns true if the pill's geometry changed (so the interaction layer must follow the morph).
    bool apply(const Scene& scene);

    // Rebuilds the implicit animations from the current tokens() (after a reload).
    void refreshAnimations();

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown();
    void pressRelease();

    // Pointer feedback on one content element instead of the whole pill (see ElementVisuals).
    void setHovered(const std::string& key) { elements_.setHovered(key); }
    void pressElement(const std::string& key) { elements_.press(key); }
    void releaseElement(const std::string& key) { elements_.release(key); }
    void scrubElement(const std::string& key, float fraction) { elements_.scrub(key, fraction); }
    void endScrub(const std::string& key, std::optional<float> seekTo, Ms now) { elements_.endScrub(key, seekTo, now); }

private:
    wuc::Compositor compositor_{ nullptr };
    wuc::ShapeVisual pill_{ nullptr };
    wuc::CompositionPropertySet shape_{ nullptr }; // MaxRadius, read by the corner expressions
    GraphicsDevice device_;
    ElementVisuals elements_;
    Vec2 size_, offset_;
};

} // namespace cup::platform
