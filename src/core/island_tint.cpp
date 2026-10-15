#include "core/island_tint.h"

#include "core/tokens.h"

namespace cup {

WantedTint wantedTint(const IslandState& state)
{
    for (const Activity& activity : state.activities)
        if (activity.content.tint || activity.content.tintComing) return { activity.content.tint, activity.content.tintComing };
    return {};
}

IslandTint nextTint(IslandTint tint, const WantedTint& wanted, Ms now)
{
    if (wanted.tint) return { wanted.tint, std::nullopt };
    if (!tint.shown || !wanted.coming) return {}; // paused, gone, switched off: the theme now
    if (!tint.clearAt) tint.clearAt = now + tokens().coverTintLinger;
    if (now >= *tint.clearAt) return {};
    return tint;
}

std::optional<Ms> tintDeadline(const IslandTint& tint) { return tint.clearAt; }

} // namespace cup
