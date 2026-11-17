# 138: A task carried into another group keeps its own key

Status: Accepted (amends 129). Core tested; the look on Windows needs a run.

**Context.** Dragging a task from one group into another, the other group's
tasks looked gone and the drop outline covered all of them. Pomodoro numbers
tasks per group, so the carried task ("task-1" of A) could share its row id
with one already in B. In the preview both rows' items had the same keys
("card.group-B/task-1/…"): both followed the pointer (`follow` moves every
item under the carried row's key), the outline spanned both rows
(`recordMovables` unions them), and the renderer saw two items per key.

**Decision.** `withMove` gives a row carried into another card a name of its
own in the preview: its source card's id, ":", its id. Within its own card
it keeps its id, so it doesn't change while it's dragged there. A provider's
row ids need only be unique within their card, as they are now.
