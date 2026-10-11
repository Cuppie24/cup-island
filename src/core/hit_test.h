#pragma once
// Which action, if any, a click on the pill triggers.

#include "core/layout.h"
#include "core/provider.h"

#include <optional>

namespace cup {

struct Hit {
    Action action;
    std::string key; // the Placed element that was hit, for press feedback
};

// `point` is in pill coordinates and tested against each element's hitFrame.
// Enabled buttons return their action; a seekable progress bar returns
// "seek" with the clicked fraction. Anything else returns nothing (the click
// is then a plain pill click).
std::optional<Hit> hitTest(const Scene& scene, Vec2 point);

// Where along a progress bar `x` (pill coordinates) points, 0..1. Clamped, so
// a scrub dragged past either end (or off the pill) pins to it.
float seekFraction(const Placed& progress, float x);

// The island Event for a host-owned control (the Expanded switcher), or
// nothing for a provider's action, which goes to ProviderRegistry::route.
std::optional<Event> islandEvent(const Action& action);

} // namespace cup
