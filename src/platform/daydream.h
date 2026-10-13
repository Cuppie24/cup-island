#pragma once
// The daydream's show (decision 050): Home's shortcut held, the whole island
// (pill, bubble, neck) lifts off the screen edge, drifts, spins and wobbles
// like jelly, and a droplet buds off it and merges back through a liquid
// neck. Let go, it finishes the turn it's in and lands.
//
// One container moves, so the pill's own morphs (an alert arriving) carry on
// inside it untouched (decision 008: nothing else owns its properties). All
// motion is expressions over one property set, `Float` (0 at rest, 1 in the
// air) and `Phase` (radians: the turn, the drift, the wobble and the buds
// all repeat with it), so the compositor runs it and the app does nothing
// per frame.

#include "core/island.h"
#include "platform/liquid_bridge.h"
#include "platform/win.h"

#include <chrono>

namespace cup::platform {

class Daydream {
public:
    // `island`: the container the pill, the bubble and their neck are in;
    // `pill`: the pill's body, which it turns around and buds from.
    void build(wuc::Compositor const& compositor, wuc::ContainerVisual const& island, wuc::Visual const& pill);
    void refreshAnimations(); // after a tokens.json reload: sizes, drift, colour

    // Starts or ends it; no-op if unchanged. Ending, returns how long until
    // it has landed (the blobs flare into the edge then); starting, zero.
    Ms set(bool on);

private:
    void start();
    Ms stop();
    void spin(); // the endless loop, once spun up
    float phaseNow() const; // where Phase is: Composition never says (decision 001), so it's tracked

    using Clock = std::chrono::steady_clock;
    wuc::Compositor compositor_{ nullptr };
    wuc::CompositionPropertySet params_{ nullptr }; // Float, Phase, and the tokens the expressions read
    wuc::ShapeVisual droplet_{ nullptr };
    wuc::CompositionColorBrush brush_{ nullptr };
    LiquidBridge neck_; // between the pill and the droplet
    bool on_ = false;
    int run_ = 0;       // each start and stop: a batch finishing for an older one is ignored
    float base_ = 0;    // Phase's whole turn the current run starts from (a restart mid-landing)
    enum class Stage { Rest, SpinUp, Looping, Landing } stage_ = Stage::Rest;
    Clock::time_point since_; // when the current stage started
    float landTo_ = 0;  // Landing: the whole turn Phase settles on
};

} // namespace cup::platform
