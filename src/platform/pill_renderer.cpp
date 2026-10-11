#include "platform/pill_renderer.h"

#include "core/tokens.h"

namespace cup::platform {

void PillRenderer::build(wuc::Compositor const& c, wuc::ContainerVisual const& root, const Scene& initial)
{
    device_.create(c);
    pill_.build(c, device_, root, initial.size, initial.offset);
    bubble_.build(c, device_, root, {}, initial.bubbleTuck); // above the pill: it's dragged over it
    bridge_.build(c, root, tokens().canvas, pill_, bubble_);
    apply(initial);
}

void PillRenderer::refreshAnimations()
{
    pill_.refreshAnimations();
    bubble_.refreshAnimations();
    bridge_.refreshAnimations();
}

void PillRenderer::apply(const Scene& scene)
{
    pill_.place(scene.size, scene.offset);
    pill_.setHighlighted(scene.dropTarget);
    pill_.setContent(scene.elements, scene.now);

    // While the bubble is dragged it just follows the cursor: no neck, no
    // other motion (decision 007). The neck is for budding and settling.
    const BubbleScene* bubble = scene.bubble ? &*scene.bubble : nullptr;
    bridge_.setEnabled(!(bubble && bubble->dragCentre));
    if (scene.side) {
        // Expanded: the bubble's shape grows into the side card listing the others (decision 007).
        const Rect& f = scene.side->frame;
        bubble_.follow(std::nullopt);
        bubble_.place({ f.width, f.height }, { f.x, f.y });
        bubble_.setContent(scene.side->elements, scene.now);
    } else if (bubble) {
        bubble_.setRowHighlight(std::nullopt, 0);
        bubble_.place({ bubble->frame.width, bubble->frame.height }, { bubble->frame.x, bubble->frame.y });
        bubble_.follow(bubble->dragCentre);
        bubble_.setContent(bubble->elements, scene.now);
    } else {
        // No bubble: shrink into the pill (it buds back out of the same spot).
        bubble_.setRowHighlight(std::nullopt, 0);
        bubble_.follow(std::nullopt);
        bubble_.place({}, scene.bubbleTuck);
        bubble_.setContent({}, scene.now);
    }
}

} // namespace cup::platform
