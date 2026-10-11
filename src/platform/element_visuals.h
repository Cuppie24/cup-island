#pragma once
// One Composition visual per laid-out content item (core Placed), diffed by
// key: new items fade in, gone items fade out, changed items are redrawn.
// Progress bars and waveforms animate on the compositor, so a playing track
// costs no app CPU per frame (TR §5).

#include "core/layout.h"
#include "platform/graphics_device.h"

#include <map>
#include <string>
#include <vector>

namespace cup::platform {

class ElementVisuals {
public:
    void init(wuc::Compositor const& compositor, GraphicsDevice& device, wuc::ContainerVisual const& parent);
    void apply(const Scene& scene);

    // Press feedback for a single element (a button), instead of the whole pill.
    void press(const std::string& key);
    void release(const std::string& key);

private:
    struct View {
        Placed placed;
        wuc::Visual visual{ nullptr };
        wuc::CompositionDrawingSurface surface{ nullptr };               // text, icon, button, image
        wuc::CompositionRoundedRectangleGeometry fill{ nullptr };        // progress
        std::vector<wuc::SpriteVisual> bars;                             // waveform
    };

    View create(const Placed& placed, Ms now);
    void update(View& view, const Placed& placed, Ms now);
    void paint(View& view);
    void animateProgress(View& view, Ms now);
    void animateWaveform(View& view);
    void fadeOutAndRemove(wuc::Visual const& visual);

    wuc::Compositor compositor_{ nullptr };
    GraphicsDevice* device_ = nullptr;
    wuc::ContainerVisual parent_{ nullptr };
    std::map<std::string, View> views_;
    bool deviceLost_ = false;
};

} // namespace cup::platform
