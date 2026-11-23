#pragma once
// The toast (decision 071): a pill floating over the bottom of the card,
// above its content, its text and button on it (Scene::toast). Like a toast
// from a toaster, it slides up from behind the card's bottom edge as it
// fades in, and back down as it fades out, over toastSlideDuration.

#include "core/layout.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <cstdint>
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
    void refresh(bool coloursOnly);                            // tokens changed: its fill fades, its content follows
    ElementVisuals& elements() { return elements_; }           // hover and press on its button

private:
    void animate(bool in, float slide);
    void applyShadow();        // toastShadowBlur, -Offset, -Color
    void paintShadowMask(Vec2 size, float radius); // the pill's shape for the shadow, redrawn when it changes

    wuc::Compositor compositor_{ nullptr };
    wuc::ContainerVisual frame_{ nullptr };                   // still, clipped to the card
    wuc::ContainerVisual layer_{ nullptr };                   // slides and fades as one: the pill and its content
    // The shadow (decisions 151, 165): a sprite under the pill that draws nothing itself
    // and casts a DropShadow masked by the pill's shape, as raised art's (decision 094).
    wuc::SpriteVisual caster_{ nullptr };
    wuc::DropShadow shadow_{ nullptr };
    wuc::CompositionDrawingSurface shadowMask_{ nullptr };
    GraphicsDevice* device_ = nullptr;
    Vec2 maskSize_{};             // what shadowMask_ was last painted for...
    float maskRadius_ = 0;
    std::uint64_t maskGeneration_ = 0; // ...and on which device (decision 164)
    wuc::ShapeVisual background_{ nullptr };
    wuc::CompositionRoundedRectangleGeometry shape_{ nullptr };
    wuc::CompositionColorBrush fill_{ nullptr };
    wuc::CompositionColorBrush edge_{ nullptr }; // trackColor, as the menu's
    ElementVisuals elements_;
    bool shown_ = false;
    float slide_ = 0; // the shown one's: how far it goes back down
};

} // namespace cup::platform
