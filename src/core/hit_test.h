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

// `point` is in pill coordinates. Enabled buttons return their action; a
// seekable progress bar returns "seek" with the clicked fraction. Anything
// else returns nothing (the click is then a plain pill click).
std::optional<Hit> hitTest(const Scene& scene, Vec2 point);

} // namespace cup
