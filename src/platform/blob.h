#pragma once
// One black island shape with its content: the pill, or the Split bubble.
// Both hang from the top edge (straight top flaring into the edge, rounded
// bottom; decision 025), so content nests in their corners the same way. Sets targets only; implicit
// animations do the motion (TR §5). As glass (decision 089), the blurred
// desktop shows through its fill.

#include "core/layout.h"
#include "core/motion.h"
#include "core/tokens.h"
#include "platform/badge_dots.h"
#include "platform/toast_layer.h"
#include "platform/element_visuals.h"
#include "platform/glass.h"
#include "platform/graphics_device.h"
#include "platform/win.h"

#include <optional>
#include <string>
#include <vector>

namespace cup::platform {

// The island's fill (decision 054): a left-to-right gradient from
// islandColor to islandSecondColor (the same colour unless a cover has
// two), and each ear in its end's colour, so the flared corners join
// without a seam. Static: only a recolour animates it.
struct ShapeFill {
    wuc::CompositionLinearGradientBrush gradient{ nullptr };
    wuc::CompositionColorGradientStop left{ nullptr }, right{ nullptr };
    wuc::CompositionColorBrush leftEar{ nullptr }, rightEar{ nullptr };
};

class Blob {
public:
    void build(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent, Vec2 size, Vec2 offset);
    // Tokens changed. Every colour fades to its new value (decision 051);
    // coloursOnly: content is recoloured in place, else rebuilt (new sizes).
    void refreshAnimations(bool coloursOnly);

    // Morphs there along `motion` (decision 080); no-op if unchanged. Following: lands there later.
    void place(Vec2 size, Vec2 offset, const Motion& motion);
    // Stay centred on the cursor (dragged); nullopt springs back to place(),
    // carrying `fling` (pixels per ms: how fast it was let go) into the landing.
    void follow(std::optional<Vec2> centre, Vec2 fling = {});
    // Lifted (decision 026): its centre at this x, its top liftDrop below the
    // edge, round and without ears; nullopt lands it at place(), like follow().
    void lift(std::optional<float> centreX, Vec2 fling = {});
    void setEars(bool left, bool right);     // false: that side is flush with the screen edge, no ear (decision 026)
    void setHighlighted(bool on);            // drop target: the background lightens (decision 007)
    // `reveal`: new content waits this long (the morph's, decision 080).
    void setContent(const std::vector<Placed>& elements, Ms now, Ms reveal) { elements_.apply(elements, now, reveal); }
    ElementVisuals& elements() { return elements_; } // hover, press, scrub feedback on content
    // Expanded's tabs (pill coordinates; nullopt = none): the active tab's
    // background slides to a newly picked tab; hover is fainter and just fades.
    void setSelectedTab(std::optional<Rect> tab, float cornerRadius);
    void setPinnedTab(std::optional<Rect> pin, float cornerRadius); // the pin while on: lit like the active tab (decision 046)
    void setHovered(std::optional<Rect> area, float cornerRadius); // a tab or a Home tile under the pointer
    // The keyboard's focus (decision 033): a ring inside `area`, over the
    // content, so it shows which one Enter clicks even with the pointer elsewhere.
    void setFocusRing(std::optional<Rect> area, float cornerRadius);
    // Home's tiles (decision 009): a faint background under each, always shown;
    // hover adds to it. Tabs have none: only hover and selection (decision 072).
    void setTiles(const std::vector<Rect>& tiles, float cornerRadius) { showMarks(tiles_, tiles, cornerRadius); }
    // Notification dots (decision 038), over the content: Scene::badges.
    void setBadges(const std::vector<Vec2>& centres) { badges_.show(centres); }
    // A toast floating over the content (decision 071): Scene::toast; nullopt fades it out.
    void setToast(const std::optional<ToastScene>& toast, Ms now) { toast_.show(toast, now); }
    ElementVisuals& toastElements() { return toast_.elements(); } // hover and press on its button

    // A floating card (decision 081): off the screen edge for good, every
    // corner `radius` (a window's, floatCornerRadius) and no ears, from the
    // start (no fold animation). Again with a new radius after a tokens reload.
    void detach(float radius);

    // A card flowing out of the pill, or back into it (decision 081):
    // there now, with no morph, hanging from the edge (the island's flared
    // top and corners) or off it with `radius` corners...
    void snapTo(Vec2 size, Vec2 offset);
    void setShapeNow(bool attached, float radius);
    // ...then off the edge or onto it as the island lifts and lands (decision
    // 026: the ears fold first), its corners easing to `radius` over `duration`.
    void flowShape(bool attached, float radius, Ms duration);

    // Tactile click feedback: squash on press, spring back on release.
    void pressDown();
    void pressRelease();
    // A new alert in a pill that keeps its size: swells and springs back (core alertPulses).
    void pulse();

    wuc::ShapeVisual const& body() const { return body_; } // for the liquid bridge's expressions

private:
    void playScale(float scale, Ms duration, Bezier curve);
    void setAttached(bool on); // to the screen edge: flared top corners; off it (dragged): round
    void startFollowing(std::wstring const& expression); // Offset follows drag_ (the cursor)
    void land(Vec2 fling);                               // ...until let go: springs to offset_

    wuc::Compositor compositor_{ nullptr };
    wuc::ShapeVisual body_{ nullptr };             // Size/Offset morph implicitly
    // Its shadow (decision 018): a sprite under the body that draws nothing
    // itself and casts a DropShadow masked by a live snapshot of the body's outline, so
    // the shadow has the body's shape through every morph, drag and press.
    // Null where Windows can't snapshot a visual (before 1903): no shadow.
    wuc::SpriteVisual caster_{ nullptr };
    wuc::ShapeVisual outline_{ nullptr }; // the snapshot's source: the body's shape without its content
    // ...in a layer of its own. As glass it's the island's see-through tint,
    // over the glass: the layer is made see-through as one (glassShare), so
    // where its ears overlap the body nothing is tinted twice, and the body's
    // own fill is clear. The snapshot draws the twin as its own root, without
    // the layer's opacity or scale (decision 097).
    wuc::LayerVisual twin_{ nullptr };
    wuc::CompositionVisualSurface snapshot_{ nullptr }; // ...live: the shadow's mask, and the glass's
    wuc::DropShadow shadow_{ nullptr };
    void buildShadow(wuc::Compositor const& compositor, wuc::ContainerVisual const& parent);
    void applyShadow(); // colour, blur, offset from tokens
    // Glass (decision 089): a sprite between the outline and the body showing
    // the desktop behind, blurred, cut to the outline's snapshot. Made the
    // first time glass is on (only Windows 11 offers it); faded out when off,
    // so the outline under it is the solid island again. The rim: a stroke
    // just inside the body's edge, see-through but for glass.
    wuc::SpriteVisual glass_{ nullptr };
    Look glassLook_ = Look::Plain; // how its brightness is kept in hand (glass.h)
    Darken darkenBuilt_;           // ...Darkened with these (glassDarken*): made again when they change
    wuc::CompositionBrush glassMask_{ nullptr }, glassLuminosity_{ nullptr };
    // The brightness colour, painted on a little surface: the one kind of
    // colour input Windows takes into the effect. Repainted (not faded) when it changes.
    wuc::CompositionDrawingSurface luminosity_{ nullptr };
    Color luminosityPainted_{ 0, 0, 0, 0 };
    void paintLuminosity(Color color);
    GraphicsDevice* device_ = nullptr; // build()'s: draws the brightness surface
    bool glassTried_ = false; // made once, or failed once: not tried again
    bool glassShown_ = false;
    wuc::CompositionSpriteShape rim_{ nullptr };
    wuc::CompositionColorGradientStop rimTop_{ nullptr }, rimBottom_{ nullptr };
    void buildGlass();
    // The glow (decision 090): the cover's colour (islandGlowColor) along the
    // body's bottom edge, fading out glassGlowHeight px up along glowFade's
    // curve, from the bottom stop up. Clear unless glass has a cover's
    // colours.
    std::vector<wuc::CompositionColorGradientStop> glow_;
    void buildGlow(wuc::Compositor const& compositor, wuc::CompositionGeometry const& outline);
    void fadeGlow(Ms duration);
    void buildRim(wuc::Compositor const& compositor);
    void applyGlass(Ms duration); // tokens().glass: on or off, and the rim's colour and width
    wuc::CompositionPropertySet shape_{ nullptr }; // MaxRadius, FlareRadius, Attached, Flare, Left/RightEar: read by the shape expressions
    wuc::CompositionPropertySet drag_{ nullptr };  // Centre: the cursor, while following; Drop: lifted, below the edge
    ShapeFill fill_;  // the body's: clear over the glass, the twin tints it (decision 097)
    ShapeFill solid_; // the outline's: always opaque, so its snapshot is the whole shape
    void fadeFill(Ms duration); // to the current colours (highlighted: the highlight's, both ends)
    wuc::ImplicitAnimationCollection implicit_{ nullptr }; // the Size/Offset morph, along built_
    Motion motion_; // the latest morph's: a landing uses it too
    Motion built_;  // the one implicit_ runs: rebuilt only when another is needed
    wuc::ContainerVisual content_{ nullptr };
    struct Mark { // a rounded highlight under content, made on first use
        wuc::ShapeVisual visual{ nullptr };
        wuc::CompositionRoundedRectangleGeometry shape{ nullptr };
        wuc::CompositionColorBrush brush{ nullptr };
        std::optional<Rect> at;
    };
    void showMark(Mark& mark, std::optional<Rect> at, float cornerRadius, const Color& color, bool slide);
    void showMarks(std::vector<Mark>& marks, const std::vector<Rect>& at, float cornerRadius); // tileColor, one per rect
    Mark selectedTab_, pinnedTab_, hovered_;
    struct Ring { // the focus ring: stroked, not filled; over content_, not under it
        wuc::ShapeVisual visual{ nullptr };
        wuc::CompositionRoundedRectangleGeometry shape{ nullptr };
        wuc::CompositionSpriteShape stroke{ nullptr };
        wuc::CompositionColorBrush brush{ nullptr };
        std::optional<Rect> at;
        float radius = 0;
    } focusRing_;
    std::vector<Mark> tiles_; // grows to the most shown; spare ones stay faded out
    BadgeDots badges_;
    ToastLayer toast_;
    ElementVisuals elements_;
    std::optional<float> detachedRadius_; // a floating card's corners (detach())
    Vec2 size_, offset_;
    Vec2 centre_;            // last cursor position while following
    bool following_ = false; // Offset is driven by the cursor expression, not set directly
    bool lifted_ = false;    // ...the lift's (top at liftDrop), not follow()'s (centred on the cursor)
    bool leftEar_ = true, rightEar_ = true;
    bool highlighted_ = false;
};

} // namespace cup::platform
