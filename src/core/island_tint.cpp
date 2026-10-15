#include "core/island_tint.h"

#include "core/tokens.h"

namespace cup {

std::optional<CoverTint> wantedTint(const IslandState& state)
{
    for (const Activity& activity : state.activities)
        if (activity.content.tint) return activity.content.tint;
    return std::nullopt;
}

IslandTint nextTint(IslandTint tint, std::optional<CoverTint> wanted, Ms now)
{
    if (wanted) return { wanted, std::nullopt };
    if (!tint.shown) return {};
    if (!tint.clearAt) tint.clearAt = now + tokens().coverTintLinger;
    if (now >= *tint.clearAt) return {};
    return tint;
}

std::optional<Ms> tintDeadline(const IslandTint& tint) { return tint.clearAt; }

std::shared_ptr<const ImageData> wantedBackdrop(const IslandState& state)
{
    for (const Activity& activity : state.activities)
        if (activity.content.backdrop) return activity.content.backdrop;
    return nullptr;
}

IslandBackdrop nextBackdrop(IslandBackdrop backdrop, std::shared_ptr<const ImageData> wanted, Ms now)
{
    if (wanted) return { std::move(wanted), std::nullopt };
    if (!backdrop.shown) return {};
    if (!backdrop.clearAt) backdrop.clearAt = now + tokens().coverTintLinger;
    if (now >= *backdrop.clearAt) return {};
    return backdrop;
}

} // namespace cup
