#pragma once
// Which colour the island takes on, and when (decisions 051, 058): the first
// ranked activity that asks (Content::tint: media playing with a coloured
// cover). Paused or gone, the theme comes back at once; a new track whose
// cover isn't read yet keeps the colour shown a moment, so skipping doesn't
// flash the theme.

#include "core/island.h"

#include <optional>

namespace cup {

// What the best-ranked activity asking wants: a colour now, or one coming.
struct WantedTint {
    std::optional<CoverTint> tint;
    bool coming = false; // Content::tintComing
    bool operator==(const WantedTint&) const = default;
};
WantedTint wantedTint(const IslandState& state);

struct IslandTint {
    std::optional<CoverTint> shown; // the island is in this cover's palette; none: the theme's
    std::optional<Ms> clearAt;  // ...a colour is coming but late: back to the theme at this time
    bool operator==(const IslandTint&) const = default;
};

// A colour asked for shows at once. None asked for: the theme at once,
// unless one is coming, which waits up to coverTintLinger for it.
IslandTint nextTint(IslandTint tint, const WantedTint& wanted, Ms now);

// When nextTint must run again: the wait's end, if one is running.
std::optional<Ms> tintDeadline(const IslandTint& tint);

} // namespace cup
