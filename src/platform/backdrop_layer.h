#pragma once
// The blurred cover behind the island's content (decision 055): a still
// picture (blurred once by core's blurredCover), clipped to the island's
// shape and shown faintly over its colour, so text stays readable. A new
// one crossfades over the old over the colour fade's duration; nothing
// runs in between, so it costs nothing while music plays.

#include "core/content.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <memory>

namespace cup::platform {

class BackdropLayer {
public:
    // Goes into `body` just under `content`, clipped to the island's outline.
    void build(wuc::Compositor const& c, GraphicsDevice& device, wuc::ShapeVisual const& body,
               wuc::CompositionPropertySet const& shape, wuc::Visual const& content);
    void show(const std::shared_ptr<const ImageData>& image); // nullptr: fades out
    void refresh(); // tokens changed: the strength for a dark or light theme

private:
    wuc::Compositor compositor_{ nullptr };
    GraphicsDevice* device_ = nullptr;
    wuc::ShapeVisual body_{ nullptr };
    wuc::ContainerVisual layer_{ nullptr };
    wuc::SpriteVisual current_{ nullptr };
    std::shared_ptr<const ImageData> shown_;
};

} // namespace cup::platform
