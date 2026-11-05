#pragma once
// A row's open menu (decision 086): a rounded panel over the island in the
// raised colour toasts use, its lit item, lines between groups, and its
// labels and shortcuts (Scene::menu). In the render layer's root, above the
// pill and the bubble, so it isn't clipped by the island's shape. It grows
// and fades in from where it opened over menuOpenDuration, and goes at once.

#include "core/layout.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <optional>

namespace cup::platform {

class MenuLayer {
public:
    void build(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& root);
    // `offset`: the pill's (Scene::offset): the menu's frames are in pill coordinates.
    void show(const std::optional<MenuScene>& menu, Vec2 offset, Ms now);
    void refresh(bool coloursOnly); // tokens changed: its colours fade, its content follows

private:
    wuc::Compositor compositor_{ nullptr };
    wuc::ContainerVisual layer_{ nullptr };        // at the menu's frame; scales and fades as one
    wuc::ShapeVisual shapes_{ nullptr };           // its background and lit item
    wuc::ShapeVisual lines_{ nullptr };            // the lines between its groups: cleared and drawn anew each time
    wuc::CompositionRoundedRectangleGeometry background_{ nullptr }, lit_{ nullptr };
    wuc::CompositionSpriteShape litShape_{ nullptr };
    wuc::CompositionColorBrush fill_{ nullptr }, edge_{ nullptr }, litFill_{ nullptr }, lineFill_{ nullptr };
    ElementVisuals elements_;                      // its labels and shortcuts
    std::optional<std::string> shownRow_;          // the row whose menu is up: another's opens afresh
};

} // namespace cup::platform
