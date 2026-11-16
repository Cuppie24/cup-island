# 117: Pomodoro's window redesigned: a rail, the timer across the top, tasks in groups

Status: Accepted (amends 104: a window can have a rail, a header, pages of
cards and a maximize button; amends 106: the timer page; finishes 107's
window, so the card's button into it shows). Core and provider tested; the
look and the resize on Windows need a run.

**Context.** The user asked for a full redesign of the timer's window: tasks
in groups, several groups side by side on a wide screen, a group renamed by
clicking its name like a task, a new group's button, a task's delete on
hover, a collapsed sidebar, a window that maximizes, and a resize that
doesn't lag. The window was one size (`resizable = false`), its sidebar
200 px of names, its to-dos one list.

**Decision.**
- **What a window can say** (`core/window_content.h`):
  - `WindowContent::rail`: the sidebar is `windowRailWidth` of the pages'
    glyphs alone; the window's title moves to its title bar.
    `WindowPage::atFoot` puts a page at the rail's foot (the timer's settings).
  - `WindowPage::header`: rows across the whole pane, over the page, a line
    under them edge to edge (`WindowScene::headerRule`, one sprite the
    platform resizes). It scrolls with the page, so keys, fields and clicks
    work there as on the rest of it. `titled = false`: no page title, the
    header says what the page is.
  - `WindowPage::cards`: its sections are cards, `windowCardWidth` wide,
    `windowCardGap` apart, as many to a row as fit, each row as tall as its
    tallest card (`core/window_page.h`: `cardGrid`, `cardFrames`). A card's
    title sits inside it, on its background.
  - `WindowSection::id` names a section's keys, so removing one doesn't
    rebuild the ones after it; `titleEdit` makes its title editable in
    place (decision 043); `titleItems` go beside the title (a count),
    `titleTrailing` at its end (a delete shown on hover).
- **Maximize.** A resizable, maximizable window gets a third chrome
  button (`kWindowMaximize`, `Glyph::Maximize`, `Glyph::Restore` while
  maximized, which `windowLayout` is told). Double clicking the bar,
  dragging it to the screen's top and Win+Up were already Windows'.
- **Resizing without lag.** Cards have one width whatever the window's:
  a resize only changes their offsets, which Composition moves without
  repainting a surface; only a change in the number of columns moves cards
  to another row. Their origins are whole pixels, so no item is resnapped
  (decision 090). Below one card's width, the one column narrows and its
  text is repainted, as any page's below `windowPageMaxWidth`. `ElementVisuals::apply`
  found removed visuals by a search per visual (n² on every frame); a set now.
  The window's title is given to Windows only when it changes.
- **The timer page** (`providers/pomodoro/pomodoro_window`): the header is
  the time left (`Hero`), stop, ▶/⏸ (the filled circle) and skip, and at its
  right the phase and round and the session's steps. Dormant: the planned
  focus, ▶, stop and skip dimmed in their places, so nothing moves when it
  starts. Stop asks in place of the controls. Under it, a card per group,
  then an Undo card while one shows, then "New group". Today's line left
  the timer page: the statistics page has today.
- **Groups** (`providers/pomodoro/task_board`, pure): named groups, each a
  `TodoList` (decisions 016, 071). Actions in a group are its list's,
  prefixed: `tasks.<group>:todo.toggle.5`. A card: its name (edited in
  place, `taskGroupNameMaxLength`), the open count beside it, a delete on
  hover; "Add task" (a field with a +), then its tasks, open ones first,
  solved ones struck after; a task's delete shows on hover. A group is
  deleted at once with an Undo, its tasks with it. There is always a
  group: deleting the last leaves an empty "Tasks", which goes again if
  that delete is undone while it's still empty. New ones are "Group n".
- **Saved** under the same key: "# <name>" over each group's
  `TodoList::save`. A list saved before groups loads as one "Tasks" group.

**Not taken.** Cards stretching to fill the row: every frame of a resize
would repaint every card's text, the lag the user saw. A pinned header that
doesn't scroll: the keys, fields and hover would need a third area beside
the page and the footer, for a header that's short anyway. Clear solved /
Clear all in the window: a group's delete with Undo covers clearing; the
Done row's hiding: solved tasks show struck at the group's end, as in the
user's design. A dashed outline on "New group": the painters have no
dashed stroke; it's a plain card. Snap Layouts on the maximize button
(`HTMAXBUTTON`): it moves its clicks to the non-client area; left for a run on Windows.

**Consequences.** `PomodoroQuestion` keeps Stop and ResetStats. The card
fixture (`tests/todo_card_fixture.h`) still covers `appendTodoRows`. The
header doesn't wrap: in a window narrower than its row it's laid out as
usual, past the pane's edge.
