#pragma once
// One Composition visual per laid-out content item (core Placed), diffed by
// key: new items fade in, gone items fade out, changed items are redrawn.
// Progress bars, waveforms and swimming text animate on the compositor, so a
// playing track costs no app CPU per frame (TR §5).

#include "core/layout.h"
#include "platform/element_art.h"
#include "platform/graphics_device.h"
#include "platform/list_viewports.h"
#include "platform/switch_parts.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup::platform {

class ElementVisuals {
public:
    void init(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent);
    void apply(const std::vector<Placed>& elements, Ms now);
    // Tokens changed. Colours only (a theme, a cover): shapes fade to the new
    // colours and text is redrawn in place. Anything else (tokens.json, display
    // scale): every visual is dropped, so the next apply() rebuilds it.
    void refresh(bool coloursOnly);

    // Pointer feedback on content (decision 002), by element key. Unknown keys are ignored.
    void setHovered(const std::string& key); // "" = nothing; buttons light up, progress shows its knob
    void setHoveredRow(const std::string& row); // "<slot>/<row>" under the pointer, "" = none: reveals its revealOnHover buttons
    void press(const std::string& key);      // buttons
    void release(const std::string& key);
    void scrub(const std::string& key, float fraction); // progress follows the pointer and ignores updates
    void endScrub(const std::string& key, std::optional<float> seekTo, Ms now); // nullopt = cancelled

private:
    struct View {
        Placed placed;
        wuc::Visual visual{ nullptr };
        float lead = 0; // the visual starts this far left of the frame (knob overhang, marquee fade)
        wuc::CompositionDrawingSurface surface{ nullptr };          // text, icon, button glyph, image
        float marqueeWidth = 0;                                     // > 0: swimming text, one copy's width
        wuc::Visual highlight{ nullptr };                           // button hover shape
        ArtParts art;                                               // clickable art: hover dim + "open" glyph
        wuc::CompositionDrawingSurface glyphSurface{ nullptr };     // that glyph
        wuc::SpriteVisual picture{ nullptr };                       // an Image's: a new picture fades in over it
        wuc::CompositionRoundedRectangleGeometry fill{ nullptr };   // progress bar
        wuc::CompositionEllipseGeometry arc{ nullptr };             // progress ring
        wuc::CompositionSpriteShape knob{ nullptr };                // progress
        std::vector<wuc::CompositionRoundedRectangleGeometry> bars; // waveform
        wuc::ShapeVisual waveform{ nullptr };
        bool scrubbing = false;
        // Shape colours, kept so a recolour fades them (decision 051); null when unused.
        wuc::CompositionColorBrush track{ nullptr }, accent{ nullptr }, knobBrush{ nullptr }, hover{ nullptr };
        wuc::CompositionColorBrush base{ nullptr };                 // a backed button's disc (decision 064)
        SwitchParts toggle; // a Switch's track and knob (decision 053)
    };

    View create(const Placed& placed, Ms now, bool fade = true); // fade: fades in (not when a text turns into its field)
    bool update(View& view, const Placed& placed, Ms now); // true = rebuilt
    void paint(View& view);
    void recolor(View& view); // fades its shapes to the current colours and redraws its pixels
    void animateBars(View& view);
    void showHover(const std::string& key, bool on);
    void fadeOutAndRemove(wuc::Visual const& visual);
    void place(View& view); // its offset, in its list's scroller or the content

    wuc::Compositor compositor_{ nullptr };
    GraphicsDevice* device_ = nullptr;
    wuc::ContainerVisual parent_{ nullptr };
    std::map<std::string, View> views_;
    ListViewports lists_; // scrolling lists' clipped, eased containers (decision 029)
    std::string hovered_;
    std::string hoveredRow_;
    bool deviceLost_ = false;
};

} // namespace cup::platform
