#include "core/hit_test.h"

#include <algorithm>
#include <string>

namespace cup {

std::optional<Hit> hitTest(const Scene& scene, Vec2 point)
{
    for (const Placed& p : scene.elements) {
        if (!p.frame.contains(point)) continue;
        if (auto* button = std::get_if<Button>(&p.item); button && button->enabled)
            return Hit{ { p.providerId, p.activityId, button->actionId, 0 }, p.key };
        if (auto* progress = std::get_if<Progress>(&p.item); progress && progress->seekable && p.frame.width > 0)
            return Hit{ { p.providerId, p.activityId, "seek", std::clamp((point.x - p.frame.x) / p.frame.width, 0.0f, 1.0f) }, p.key };
    }
    return std::nullopt;
}

std::optional<Event> islandEvent(const Action& action)
{
    if (!action.providerId.empty()) return std::nullopt;
    if (action.actionId == kIslandNext) return ExpandedPaged{ 1 };
    if (action.actionId == kIslandPromote) return ActivityPromoted{ action.activityId };
    return std::nullopt;
}

} // namespace cup
