# 156: "New group" sits out a group's drag

Status: Accepted (amended). Core tested; the look needs a Windows run.

**Context.** While a group was carried on the Timer page, "New group" counted
as a card on the hovered group's row and made the drop read left to right
(decision 135). It's a button that adds a card, not one of them (decisions
122, 153); the user didn't want it in the drag.

The first take (0.12.16) left it out of the preview altogether. Gone, it left
a hole the groups slid into, so the page under the pointer changed and with
it where the carried group landed.

**Decision.** It's shown, in its place, last, while a group is carried: the
preview keeps it, so the grid doesn't shift. It isn't included in the drop:
it isn't movable, and `cardTarget` doesn't count a dashed card
(`WindowSection::dashed`) as one beside the hovered card.

**Consequences.** `tests/window_drag_tests.cpp`.
