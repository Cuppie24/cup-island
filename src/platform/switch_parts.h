#pragma once
// An on/off switch (decision 053): a pill-shaped track and a round knob that
// slides to the end that's on. On, the track takes the live colour and the
// knob the island's, so it reads in any theme or cover's colours.

#include "core/content.h"
#include "platform/win.h"

namespace cup::platform {

struct SwitchParts {
    wuc::ShapeVisual visual{ nullptr };
    wuc::CompositionEllipseGeometry knob{ nullptr };
    wuc::CompositionColorBrush track{ nullptr }, knobBrush{ nullptr };
};

SwitchParts createSwitch(wuc::Compositor const& c, Vec2 frame, bool on);

// Flipped: the knob slides and both colours follow, quickly (it answers a click).
void slideSwitch(wuc::Compositor const& c, const SwitchParts& parts, Vec2 frame, bool on);

Color switchTrackColor(bool on);
Color switchKnobColor(bool on);

} // namespace cup::platform
