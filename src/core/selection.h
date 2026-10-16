#pragma once
// Picking things in a card's list like files in Explorer (decision 064): the
// host keeps what's selected, providers are told. Part of reduce() and
// layout(); pure.

#include "core/layout.h"

#include <string>
#include <vector>

namespace cup {

// The ids of the things that can be picked in a card's list, in its order (Row::selectable).
std::vector<std::string> selectableItems(const IslandState& state, const std::string& activityId, const std::string& list);

void selectItem(IslandState& state, const ItemSelected& e, Ms now);  // only on the shown card, only its things
void setSelection(IslandState& state, const SelectionSet& e, Ms now);

// After every event: a selection goes with its card, and keeps only things still there.
void settleSelection(IslandState& state);

bool isSelected(const IslandState& state, const std::string& activityId, const std::string& list, const std::string& id);

// Marks the selected rows' and cells' backgrounds on the scene.
void markSelection(const IslandState& state, Scene& scene);

} // namespace cup
