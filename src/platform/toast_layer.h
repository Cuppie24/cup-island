#pragma once
// The toast (decision 071): a pill floating over the bottom of the card,
// above its content, its text and button on it (Scene::toast). Like a toast
// from a toaster, it slides up from behind the card's bottom edge as it
// fades in, and back down as it fades out, over toastSlideDuration.

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
    // The still frame it slides in: clipped to it, so the toast hides below the card's
    // bottom edge. The blob binds its size to the card's.
    wuc::ContainerVisual const& frame() const { return frame_; }
    void show(const std::optional<ToastScene>& toast, Ms now); // nullopt: it slides back down, fading out
    // On the island (not a provider's window): its edge shines on liquid glass (decision 165). After build().
    void setShiny();
    void refresh(bool coloursOnly);                            // tokens changed: its fill fades, its content follows
    ElementVisuals& elements() { return elements_; }           // hover and press on its button

private:
    void animate(bool in, float slide);
    void applyShadow(); // toastShadowBlur, -Offset, -Color

    wuc::Compositor compositor_{ nullptr };
    wuc::ContainerVisual frame_{ nullptr };                   // still, clipped to the card
    wuc::ContainerVisual layer_{ nullptr };                   // slides and fades as one: the pill and its content
    wuc::LayerVisual pillLayer_{ nullptr };                   // the pill, casting its shadow (decision 151)
    wuc::DropShadow shadow_{ nullptr };
    wuc::ShapeVisual background_{ nullptr };
    wuc::CompositionRoundedRectangleGeometry shape_{ nullptr };
    wuc::CompositionColorBrush fill_{ nullptr };
    wuc::CompositionColorBrush edge_{ nullptr }; // trackColor, as the menu's
    Shine shine_;                                // over it on liquid glass: a shiny edge (decision 165)
    ElementVisuals elements_;
    bool shown_ = false;
    float slide_ = 0; // the shown one's: how far it goes back down
};

} // namespace cup::platform
