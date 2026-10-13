#pragma once
// Scrolling, reorderable lists inside a slot (decision 029). Pure: where each
// row of a slot goes once lists are windowed, scrolled and a row is dragged,
// and the state rules for the wheel, reveals and drags.

#include "core/island.h"
#include "core/layout.h"

#include <optional>
#include <string>
#include <vector>

namespace cup {

// Whose lists these are, and how they're scrolled and dragged now.
struct ListState {
    std::string activityId;
    const std::map<std::string, ListScroll>* scrolls = nullptr;
    const ListDrag* drag = nullptr;
};

struct RowSpot {
    float y = 0;                // the row's top, down from the slot's content top: scrolled, or dragged
    std::optional<InList> list; // in a list: its window (y from the content top too) and place
};

struct RowPlan {
    std::vector<RowSpot> rows;
    float height = 0; // the slot's content height: a list counts its window, not every row
};

// `heights`: each row's natural height. Rows outside a list stack as before.
// `windowHeights`: what a list's window is sized by (its rows on one line,
// so a wrapped one doesn't stretch it, decision 045); none: `heights`.
RowPlan planRows(const Slot& slot, const std::vector<float>& heights, const ListState& state,
                 const std::vector<float>* windowHeights = nullptr);

std::string listKey(const std::string& activityId, const std::string& list); // "<activityId>/<list>"

void scrollList(IslandState& s, const ListScrolled& e, Ms now);
void revealListRow(IslandState& s, const ListRevealed& e);
void startListDrag(IslandState& s, const ListDragStarted& e, Ms now);
void moveListDrag(IslandState& s, const ListDragMoved& e, Ms now);

} // namespace cup
