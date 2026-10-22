#pragma once
// The host's toast (decision 071): a short note with one action at the
// bottom of its card, gone by itself after a while. Pure.

#include "core/island.h"

namespace cup {

// The toast on this card, if one is up.
const Toast* toastOn(const IslandState& state, const std::string& activityId);

// The card with its toast as the last row: its text quiet, its action a
// button, the two centred as one, a group of its own. A stacked card's
// (Centred, List): the card grows by the row while it's up.
Content withToast(Content card, const Toast& toast);

} // namespace cup
