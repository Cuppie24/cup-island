#pragma once
// The scene of the card in front of the user, wherever it is: the island's
// (Expanded, Home), or a floating card's (decision 081), for tests that
// look at a card's content whether it floats or not.

#include "core/float_layout.h"
#include "core/floating.h"
#include "core/layout.h"

namespace cup::test {

inline Scene cardScene(const IslandState& state, Ms now)
{
    if (isExpanded(state) || state.floats.empty()) return layout(state, now);
    const FloatingCard* card = focusedFloat(state);
    return floatScene(state, card ? *card : state.floats.back(), now);
}

} // namespace cup::test
