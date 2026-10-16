#pragma once
// One black island shape with its content: the pill, or the Split bubble.
// Both hang from the top edge (straight top flaring into the edge, rounded
// bottom; decision 025), so content nests in their corners the same way. Sets targets only; implicit
// animations do the motion (TR §5).

#include "core/layout.h"
#include "core/tokens.h"
#include "platform/badge_dots.h"
#include "platform/element_visuals.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <optional>
#include <string>
#include <vector>

namespace cup::platform {

class Blob {
public:
    void build(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent, Vec2 size, Vec2 offset);
    // Tokens changed. Every colour fades to its new value (decision 051);
    // coloursOnly: content is recoloured in place, else rebuilt (new sizes).
    void refreshAnimations(bool coloursOnly);

    void place(Vec2 size, Vec2 offset);      // morphs there; no-op if unchanged. Following: lands there later
    void follow(std::optional<Vec2> centre); // stay centred on the cursor (dragged); nullopt eases back to place()
    // Lifted (decision 026): its centre at this x, its top liftDrop below the
    // edge, round and without ears; nullopt lands it at place(), like follow().
    void lift(std::optional<float> centreX);
    void setEars(bool left, bool right);     // false: that side is flush with the screen edge, no ear (decision 026)
    void setHighlighted(bool on);            // drop target: the background lightens (decision 007)
    void setContent(const std::vector<Placed>& elements, Ms now) { elements_.apply(elements, now); }
    ElementVisuals& elements() { return elements_; } // hover, press, scrub feedback on content
    // Expanded's tabs (pill coordinates; nullopt = none): the active tab's
    // background slides to a newly picked tab; hover is fainter and just fades.
    void setSelectedTab(std::optional<Rect> tab, float cornerRadius);
    void setPinnedTab(std::optional<Rect> pin, float cornerRadius); // the pin while on: lit like the active tab (decision 046)
    void setHovered(std::optional<Rect> area, float cornerRadius); // a tab or a Home tile under the pointer
    // The keyboard's focus (decision 033): a ring inside `area`, over the
    // content, so it shows which one Enter clicks even with the pointer elsewhere.
    void setFocusRing(std::optional<Rect> area, float cornerRadius);
    // Home's tiles (decision 009) and Expanded's tabs (decision 007): a faint
    // background under each, always shown; hover and selection add to it.
    void setTiles(const std::vector<Rect>& tiles, float cornerRadius) { showMarks(tiles_, tiles, cornerRadius, tokens().tileColor); }
    void setTabs(const std::vector<Rect>& tabs, float cornerRadius) { showMarks(tabs_, tabs, cornerRadius, tokens().tileColor); }
    // Picked things in a card's list (decision 064): lit like the active tab.
    // At once, no fade: picking follows the pointer (a box being drawn), a fade lags behind it.
    void setSelectedItems(const std::vector<Rect>& items, float cornerRadius) { showMarks(selected_, items, cornerRadius, tokens().tabSelectedColor, true); }
    // The context menu's panel (decision 066): over the card, under its items; nullopt: none.
    void setMenu(std::optional<Rect> panel, float cornerRadius);
    void setMenuHovered(std::optional<Rect> item, float cornerRadius); // a menu item under the pointer: over the panel, under the items
    // The box being drawn round things to pick them; nullopt: none.
    void setSelectionBox(std::optional<Rect> box) { showMark(box_, box, tokens().focusRingWidth * 2, tokens().selectionBoxColor, false, true); }
    // Notification dots (decision 038), over the content: Scene::badges.
    void setBadges(const std::vector<Vec2>& centres) { badges_.show(centres); }

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown();
    void pressRelease();

    wuc::ShapeVisual const& body() const { return body_; } // for the liquid bridge's expressions

private:
    void playScale(float scale, Ms duration, Bezier curve);
    void setAttached(bool on); // to the screen edge: flared top corners; off it (dragged): round
    void startFollowing(std::wstring const& expression); // Offset follows drag_ (the cursor)
    void land();                                         // ...until let go: glides to offset_

    wuc::Compositor compositor_{ nullptr };
    wuc::ShapeVisual body_{ nullptr };             // Size/Offset morph implicitly
    // Its shadow (decision 018): a sprite under the body that draws nothing
    // itself and casts a DropShadow masked by a live snapshot of the body's outline, so
    // the shadow has the body's shape through every morph, drag and press.
    // Null where Windows can't snapshot a visual (before 1903): no shadow.
    wuc::SpriteVisual caster_{ nullptr };
    wuc::ShapeVisual outline_{ nullptr }; // the snapshot's source: the body's shape without its content
    wuc::DropShadow shadow_{ nullptr };
    void buildShadow(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent);
    void applyShadow(); // colour, blur, offset from tokens
    wuc::CompositionPropertySet shape_{ nullptr }; // MaxRadius, FlareRadius, Attached, Flare, Left/RightEar: read by the shape expressions
    wuc::CompositionPropertySet drag_{ nullptr };  // Centre: the cursor, while following; Drop: lifted, below the edge
    // The island's fill (decision 054): a left-to-right gradient from
    // islandColor to islandSecondColor (the same colour unless a cover has
    // two). Static: only a recolour animates it. Each ear takes its end's
    // colour, so the flared corners join without a seam.
    wuc::CompositionLinearGradientBrush gradient_{ nullptr };
    wuc::CompositionColorGradientStop leftStop_{ nullptr }, rightStop_{ nullptr };
    wuc::CompositionColorBrush leftFill_{ nullptr }, rightFill_{ nullptr };
    void fadeFill(Ms duration); // to the current colours (highlighted: the highlight's, both ends)
    wuc::ImplicitAnimationCollection implicit_{ nullptr }; // the Size/Offset morph
    wuc::ContainerVisual content_{ nullptr };
    struct Mark { // a rounded highlight under content, made on first use
        wuc::ShapeVisual visual{ nullptr };
        wuc::CompositionRoundedRectangleGeometry shape{ nullptr };
        wuc::CompositionColorBrush brush{ nullptr };
        std::optional<Rect> at;
    };
    // instant: shown and hidden at once, no fade (the selection, decision 064).
    void showMark(Mark& mark, std::optional<Rect> at, float cornerRadius, const Color& color, bool slide, bool instant = false,
                  bool overlay = false); // overlay: in overlay_, over the content
    void showMarks(std::vector<Mark>& marks, const std::vector<Rect>& at, float cornerRadius, const Color& color, bool instant = false); // one per rect
    Mark selectedTab_, pinnedTab_, hovered_, box_, menuBase_, menuWash_, menuHover_;
    wuc::ContainerVisual overlay_{ nullptr }; // over the content: the context menu (decision 066)
    struct Ring { // the focus ring: stroked, not filled; over content_, not under it
        wuc::ShapeVisual visual{ nullptr };
        wuc::CompositionRoundedRectangleGeometry shape{ nullptr };
        wuc::CompositionSpriteShape stroke{ nullptr };
        wuc::CompositionColorBrush brush{ nullptr };
        std::optional<Rect> at;
        float radius = 0;
    } focusRing_;
    std::vector<Mark> tiles_, tabs_, selected_; // grow to the most shown; spare ones stay faded out
    BadgeDots badges_;
    ElementVisuals elements_;
    Vec2 size_, offset_;
    Vec2 centre_;            // last cursor position while following
    bool following_ = false; // Offset is driven by the cursor expression, not set directly
    bool lifted_ = false;    // ...the lift's (top at liftDrop), not follow()'s (centred on the cursor)
    bool leftEar_ = true, rightEar_ = true;
    bool highlighted_ = false;
};

} // namespace cup::platform
