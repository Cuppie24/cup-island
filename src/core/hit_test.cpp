#include "core/hit_test.h"

#include <algorithm>
#include <string>

namespace cup {

std::optional<Hit> hitTest(const Scene& scene, Vec2 point)
{
    for (const Placed& p : scene.elements) {
        if (!p.hitFrame.contains(point)) continue;
        if (auto* button = std::get_if<Button>(&p.item); button && button->enabled)
            return Hit{ { p.providerId, p.activityId, button->actionId, 0 }, p.key };
        if (auto* progress = std::get_if<Progress>(&p.item); progress && progress->seekable && p.frame.width > 0)
            return Hit{ { p.providerId, p.activityId, "seek", seekFraction(p, point.x) }, p.key };
    }
    return std::nullopt;
}

float seekFraction(const Placed& progress, float x)
{
    if (progress.frame.width <= 0) return 0;
    return std::clamp((x - progress.frame.x) / progress.frame.width, 0.0f, 1.0f);
}

} // namespace cup
