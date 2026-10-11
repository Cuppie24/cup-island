#include "platform/pill_renderer.h"

#include "core/tokens.h"
#include "platform/element_painting.h"

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
    measured_.clear();
}

float PillRenderer::measure(const Text& text)
{
    if (!device_.dwrite()) return 0; // before build(): nothing laid out needs it yet
    auto key = std::make_pair(text.value, text.style);
    auto it = measured_.find(key);
    if (it != measured_.end()) return it->second;
    if (measured_.size() > 256) measured_.clear(); // titles change with every track: don't grow forever
    return measured_[key] = measureText(device_.dwrite(), text);
}

void PillRenderer::apply(const Scene& scene)
{
    pill_.place(scene.size, scene.offset);
    pill_.setHighlighted(scene.dropTarget);
    pill_.setContent(scene.elements, scene.now);
    const TabStrip* tabs = scene.tabs ? &*scene.tabs : nullptr;
    pill_.setSelectedTab(tabs && tabs->active ? std::optional<Rect>(tabs->tabs[*tabs->active].frame) : std::nullopt, tabs ? tabs->radius : 0);
    if (!tabs && scene.tiles.empty()) pill_.setHovered(std::nullopt, 0);
    std::vector<Rect> tiles;
    for (const Tile& tile : scene.tiles) tiles.push_back(tile.frame);
    pill_.setTiles(tiles, scene.tileRadius);

    // While the bubble is dragged it just follows the cursor: no neck, no
    // other motion (decision 007). The neck is for budding and settling.
    const BubbleScene* bubble = scene.bubble ? &*scene.bubble : nullptr;
    bridge_.setEnabled(!(bubble && bubble->dragCentre));
    if (bubble) {
        bubble_.place({ bubble->frame.width, bubble->frame.height }, { bubble->frame.x, bubble->frame.y });
        bubble_.follow(bubble->dragCentre);
        bubble_.setContent(bubble->elements, scene.now);
    } else {
        // No bubble: shrink into the pill (it buds back out of the same spot).
        bubble_.follow(std::nullopt);
        bubble_.place({}, scene.bubbleTuck);
        bubble_.setContent({}, scene.now);
    }
}

} // namespace cup::platform
