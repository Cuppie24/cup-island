#pragma once
// The toast (decision 071): a pill floating over the bottom of the card,
// above its content, its text and button on it (Scene::toast). It rises and
// fades in, and fades out, over toastFadeDuration.

#include "core/layout.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <optional>

namespace cup::platform {

class ToastLayer {
public:
    // Added to `parent` right above `below` (the content): it floats over it.
    void build(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent, wuc::Visual const& below);
    void show(const std::optional<ToastScene>& toast, Ms now); // nullopt: it fades out
    void refresh(bool coloursOnly);                            // tokens changed: its fill fades, its content follows
    ElementVisuals& elements() { return elements_; }           // hover and press on its button

private:
    void animate(bool in);

    wuc::Compositor compositor_{ nullptr };
    wuc::ContainerVisual layer_{ nullptr };                   // moves and fades as one: the pill and its content
    wuc::ShapeVisual background_{ nullptr };
    wuc::CompositionRoundedRectangleGeometry shape_{ nullptr };
    wuc::CompositionColorBrush fill_{ nullptr };
    ElementVisuals elements_;
    bool shown_ = false;
};

} // namespace cup::platform
