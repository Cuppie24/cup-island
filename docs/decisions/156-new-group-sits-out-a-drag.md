# 156: "New group" sits out a group's drag

Status: Accepted. Core tested; the look needs a Windows run.

**Context.** While a group was carried on the Timer page, the page was laid
out with "New group" still in the grid: it moved about as the groups made
room, and as a card on the hovered group's row it made the drop read left to
right (decision 135). It's a button that adds a card, not one of them
(decisions 122, 153); the user didn't want it in the drag.

**Decision.** `withMove`, for a card, leaves the dashed cards
(`WindowSection::dashed`) out of the preview; let go, the page is laid out as
before and it's back, last. `cardTarget` doesn't count a dashed card as one
beside the hovered card. A task carried leaves the page as it is.

**Consequences.** `tests/window_drag_tests.cpp`. While a group is carried the
page is a dashed card shorter, so it shrinks and grows back on drop.
