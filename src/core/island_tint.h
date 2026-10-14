#pragma once
// Which colour the island takes on, and when (decision 051): the first
// ranked activity that asks (Content::tint: media playing with a coloured
// cover). When none asks any more, the theme comes back after a moment, so
// the pause between two tracks doesn't flash it.

#include "core/island.h"

#include <optional>

namespace cup {

// The colour the activities ask for now: the best-ranked one's Content::tint.
std::optional<Color> wantedTint(const IslandState& state);

struct IslandTint {
    std::optional<Color> shown; // the island is in this colour's palette; none: the theme's
    std::optional<Ms> clearAt;  // ...nobody asks any more: back to the theme at this time
    bool operator==(const IslandTint&) const = default;
};

// A colour asked for shows at once; none asked for waits coverTintLinger.
IslandTint nextTint(IslandTint tint, std::optional<Color> wanted, Ms now);

// When nextTint must run again: the linger's end, if one is running.
std::optional<Ms> tintDeadline(const IslandTint& tint);

} // namespace cup
