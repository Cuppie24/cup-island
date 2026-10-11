#pragma once
// One black island shape with its own content. The pill and the Split bubble
// are both Blobs, and a Blob stays with its activity when the two trade
// roles, so the bubble the user drags is the shape that grows into the pill
// (decision 006). Sets targets only; implicit animations do the motion.

#include "core/layout.h"
#include "core/tokens.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <optional>
#include <string>
#include <vector>

namespace cup::platform {

class Blob {
public:
    // Body goes in `bodies`, content in `contents` (above every body, so a
    // shape passing over the other never hides its content).
    void build(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& bodies,
               wuc::ContainerVisual const& contents, Vec2 size, Vec2 offset, bool round);
    void refreshAnimations(); // after a tokens.json reload

    void place(Vec2 size, Vec2 offset);    // morphs there; no-op if unchanged. Following: lands there later
    void setRound(bool round);             // fully rounded (bubble, dragged) or hanging from the top edge (pill)
    void follow(std::optional<Vec2> centre); // stay centred on the cursor, even while resizing; nullopt lands
    void setContent(const std::vector<Placed>& elements, Ms now) { elements_.apply(elements, now); }

    void pressDown();
    void pressRelease();
    void pressElement(const std::string& key) { elements_.press(key); }
    void releaseElement(const std::string& key) { elements_.release(key); }

    // For the liquid bridge's expressions (its live Offset and Size).
    wuc::ShapeVisual const& body() const { return body_; }

    std::string owner; // activity this shape currently shows

private:
    void playScale(float scale, Ms duration, Bezier curve);

    wuc::Compositor compositor_{ nullptr };
    wuc::ShapeVisual body_{ nullptr };             // Size/Offset morph implicitly
    wuc::CompositionPropertySet shape_{ nullptr }; // MaxRadius, Round (0 = pill, 1 = fully rounded)
    wuc::CompositionPropertySet drag_{ nullptr };  // Centre: the cursor, while following
    ElementVisuals elements_;
    Vec2 size_, offset_;
    bool round_ = false;
    bool following_ = false; // Offset is driven by the cursor expression, not set directly
};

} // namespace cup::platform
