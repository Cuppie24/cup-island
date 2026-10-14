#include "core/island_tint.h"

#include "core/tokens.h"

namespace cup {

std::optional<Color> wantedTint(const IslandState& state)
{
    for (const Activity& activity : state.activities)
        if (activity.content.tint) return activity.content.tint;
    return std::nullopt;
}

IslandTint nextTint(IslandTint tint, std::optional<Color> wanted, Ms now)
{
    if (wanted) return { wanted, std::nullopt };
    if (!tint.shown) return {};
    if (!tint.clearAt) tint.clearAt = now + tokens().coverTintLinger;
    if (now >= *tint.clearAt) return {};
    return tint;
}

std::optional<Ms> tintDeadline(const IslandTint& tint) { return tint.clearAt; }

} // namespace cup
