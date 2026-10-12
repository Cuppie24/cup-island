#pragma once
// One black island shape with its content: the pill, or the Split bubble.
// Both hang from the top edge (straight top flaring into the edge, rounded
// bottom; decision 025), so content nests in their corners the same way. Sets targets only; implicit
// animations do the motion (TR §5).

#include "core/layout.h"
#include "core/tokens.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <optional>
#include <vector>

namespace cup::platform {

class Blob {
public:
    void build(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent, Vec2 size, Vec2 offset);
    void refreshAnimations(); // after a tokens.json reload

    void place(Vec2 size, Vec2 offset);      // morphs there; no-op if unchanged. Following: lands there later
    void follow(std::optional<Vec2> centre); // stay centred on the cursor (dragged); nullopt eases back to place()
    void setHighlighted(bool on);            // drop target: the background lightens (decision 007)
    void setContent(const std::vector<Placed>& elements, Ms now) { elements_.apply(elements, now); }
    ElementVisuals& elements() { return elements_; } // hover, press, scrub feedback on content
    // Expanded's tabs (pill coordinates; nullopt = none): the active tab's
    // background slides to a newly picked tab; hover is fainter and just fades.
    void setSelectedTab(std::optional<Rect> tab, float cornerRadius);
    void setHovered(std::optional<Rect> area, float cornerRadius); // a tab or a Home tile under the pointer
    // Home's tiles (decision 009): a faint background under each, like a hovered tab.
    void setTiles(const std::vector<Rect>& tiles, float cornerRadius);

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown();
    void pressRelease();

    wuc::ShapeVisual const& body() const { return body_; } // for the liquid bridge's expressions

private:
    void playScale(float scale, Ms duration, Bezier curve);
    void setAttached(bool on); // to the screen edge: flared top corners; off it (dragged): round

    wuc::Compositor compositor_{ nullptr };
    wuc::ShapeVisual body_{ nullptr };             // Size/Offset morph implicitly
    // Its shadow (decision 018): a sprite under the body that draws nothing
    // itself and casts a DropShadow masked by a live snapshot of the body, so
    // the shadow has the body's shape through every morph, drag and press.
    // Null where Windows can't snapshot a visual (before 1903): no shadow.
    wuc::SpriteVisual caster_{ nullptr };
    wuc::DropShadow shadow_{ nullptr };
    void buildShadow(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent);
    void applyShadow(); // colour, blur, offset from tokens
    wuc::CompositionPropertySet shape_{ nullptr }; // MaxRadius, FlareRadius, Attached: read by the corner expressions
    wuc::CompositionPropertySet drag_{ nullptr };  // Centre: the cursor, while following
    wuc::CompositionColorBrush fill_{ nullptr };
    wuc::ImplicitAnimationCollection implicit_{ nullptr }; // the Size/Offset morph
    wuc::ContainerVisual content_{ nullptr };
    struct Mark { // a rounded highlight under content, made on first use
        wuc::ShapeVisual visual{ nullptr };
        wuc::CompositionRoundedRectangleGeometry shape{ nullptr };
        wuc::CompositionColorBrush brush{ nullptr };
        std::optional<Rect> at;
    };
    void showMark(Mark& mark, std::optional<Rect> at, float cornerRadius, const Color& color, bool slide);
    Mark selectedTab_, hovered_;
    std::vector<Mark> tiles_; // grows to the most tiles shown; spare ones stay faded out
    ElementVisuals elements_;
    Vec2 size_, offset_;
    Vec2 centre_;            // last cursor position while following
    bool following_ = false; // Offset is driven by the cursor expression, not set directly
    bool highlighted_ = false;
};

} // namespace cup::platform
