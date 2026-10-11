#pragma once
// One Composition visual per laid-out content item (core Placed), diffed by
// key: new items fade in, gone items fade out, changed items are redrawn.
// Progress bars, waveforms and swimming text animate on the compositor, so a
// playing track costs no app CPU per frame (TR §5).

#include "core/layout.h"
#include "platform/graphics_device.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cup::platform {

class ElementVisuals {
public:
    void init(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent);
    void apply(const std::vector<Placed>& elements, Ms now);
    // tokens.json changed: drop every visual, so the next apply() rebuilds them with the new colours.
    void refresh();

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
        wuc::CompositionRoundedRectangleGeometry fill{ nullptr };   // progress bar
        wuc::CompositionEllipseGeometry arc{ nullptr };             // progress ring
        wuc::CompositionSpriteShape knob{ nullptr };                // progress
        std::vector<wuc::CompositionRoundedRectangleGeometry> bars; // waveform
        wuc::ShapeVisual waveform{ nullptr };
        bool scrubbing = false;
    };

    View create(const Placed& placed, Ms now);
    bool update(View& view, const Placed& placed, Ms now); // true = rebuilt
    void paint(View& view);
    void animateBars(View& view);
    void showHover(const std::string& key, bool on);
    void fadeOutAndRemove(wuc::Visual const& visual);

    wuc::Compositor compositor_{ nullptr };
    GraphicsDevice* device_ = nullptr;
    wuc::ContainerVisual parent_{ nullptr };
    std::map<std::string, View> views_;
    std::string hovered_;
    std::string hoveredRow_;
    bool deviceLost_ = false;
};

} // namespace cup::platform
