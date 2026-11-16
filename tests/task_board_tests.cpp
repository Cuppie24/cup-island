// Pomodoro's task groups (decision 117), pure: actions, saving, loading a
// list saved before groups, and never no group at all.

#include "core/tokens.h"
#include "expect.h"
#include "providers/pomodoro/task_board.h"

#include <string>

namespace cup::test {
namespace {

void boardTests()
{
    namespace task = task_action;
    namespace todo = todo_action;
    TaskBoard board;
    expect(board.groups().size() == 1 && board.groups()[0].name == "Tasks", "new: one empty group, Tasks");
    expect(board.apply(inGroup(1, todo::kAdd), "Write\nTest") && board.groups()[0].tasks.items().size() == 2, "a group's field adds its tasks");
    expect(board.apply(inGroup(1, std::string(todo::kToggle) + "1"), "") && board.groups()[0].tasks.items().back().done, "a task ticked, in its group");
    expect(!board.apply(inGroup(9, todo::kAdd), "lost") && !board.targetOf(inGroup(9, todo::kAdd)), "a group that's gone: nothing");
    expect(!board.apply("tasks.x:" + std::string(todo::kAdd), "bad") && !board.apply("tasks.1", "bad"), "a malformed action: nothing");

    expect(board.apply(task::kNewGroup, "") && board.groups().size() == 2 && board.groups()[1].name == "Group 2", "a new group at the end");
    expect(board.apply(std::string(task::kRenameGroup) + "2", "  Work  \nmore") && board.groups()[1].name == "Work", "renamed: its first line, trimmed");
    expect(!board.apply(std::string(task::kRenameGroup) + "2", "   "), "blank: unchanged");
    std::string longName(100, 'a');
    board.apply(std::string(task::kRenameGroup) + "2", longName);
    expect(board.groups()[1].name.size() == tokens().taskGroupNameMaxLength, "capped at taskGroupNameMaxLength");
    board.apply(std::string(task::kRenameGroup) + "2", "Work");
    board.apply(task::kNewGroup, "");
    expect(board.groups()[2].name == "Group 3", "named after the ones there");

    std::string saved = board.save();
    expect(saved == "# Tasks\n[x] Write\n[ ] Test\n# Work\n# Group 3\n", "saved: each group's name over its tasks");
    TaskBoard again = TaskBoard::load(saved);
    expect(again.save() == saved && again.groups().size() == 3, "loaded back the same");
    TaskBoard old = TaskBoard::load("[ ] one\n[x] two\n");
    expect(old.groups().size() == 1 && old.groups()[0].name == "Tasks" && old.groups()[0].tasks.items().size() == 2,
           "a list saved before groups: one Tasks group");
    expect(TaskBoard::load("").groups().size() == 1, "nothing saved: one empty group");

    // Deleting: back with Undo; the last one leaves an empty Tasks, which goes again on Undo.
    TaskBoard two;
    two.apply(task::kNewGroup, "");
    auto gone = two.removeGroup(1);
    expect(gone && gone->index == 0 && !gone->standIn && two.groups().size() == 1, "a group deleted");
    two.restoreGroup(*gone);
    expect(two.groups().size() == 2 && two.groups()[0].id == 1, "restored where it was");
    two.removeGroup(1);
    auto last = two.removeGroup(2);
    expect(last && last->standIn && two.groups().size() == 1 && two.groups()[0].name == "Tasks" && two.groups()[0].id == *last->standIn,
           "the last one deleted: a fresh, empty Tasks");
    two.restoreGroup(*last);
    expect(two.groups().size() == 1 && two.groups()[0].id == 2, "undone: the stand-in goes, still empty");
    auto again2 = two.removeGroup(2);
    two.apply(inGroup(*again2->standIn, todo::kAdd), "kept");
    two.restoreGroup(*again2);
    expect(two.groups().size() == 2, "a stand-in that got tasks stays");
}

} // namespace

void runTaskBoardTests() { boardTests(); }

} // namespace cup::test
