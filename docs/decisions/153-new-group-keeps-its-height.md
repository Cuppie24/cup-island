# 153: "New group" keeps its own height

Status: Accepted (amends 140). Core tested; the look needs a Windows run.

**Context.** Decision 140 stretched every card in a row to the row's tallest,
"New group" included: beside a group with a few tasks, its dashed outline
grew into a tall empty box with its one line at the top. The user didn't
want it aligned to the groups' height.

**Decision.** `cardFrames` takes, per card, whether it keeps its own height.
A dashed card (`WindowSection::dashed`: "New group", a button rather than a
card to fill) does; groups still end together. The row is still as tall as
its tallest card, so the next row starts in the same place either way.

**Consequences.** `tests/window_card_tests.cpp`.
