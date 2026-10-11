#pragma once
// Draws the pill. apply() sets target Size/Offset/Opacity and implicit
// animations do the morph (TR §5), so this never redraws per frame.

#include "core/layout.h"
#include "platform/win.h"

namespace cup::platform {

class PillRenderer {
public:
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& root, const Scene& initial);
    void apply(const Scene& scene);

    // Rebuilds the implicit animations from the current tokens() (after a reload).
    void refreshAnimations();

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown();
    void pressRelease();

private:
    wuc::Compositor compositor_{ nullptr };
    wuc::ShapeVisual pill_{ nullptr };
    wuc::ContainerVisual detail_{ nullptr }; // placeholder expanded content
};

} // namespace cup::platform
