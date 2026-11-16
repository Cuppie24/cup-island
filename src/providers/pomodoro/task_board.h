#pragma once
// Pomodoro's tasks in named groups (decision 117), pure: the groups, the
// actions on them, how they're saved. Each group's tasks are a TodoList
// (decisions 016, 071); the board only adds the groups around them. There
// is always at least one group: deleting the last leaves a new, empty one.

#include "providers/pomodoro/todo_list.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cup {

namespace task_action {
inline constexpr const char* kNewGroup = "group.new";
inline constexpr const char* kRenameGroup = "group.rename."; // + the group's id: Action::text = its new name (edited in place)
inline constexpr const char* kDeleteGroup = "group.delete."; // + the group's id: at once, with an Undo
// A to-do action (todo_action) on one group's tasks: "tasks.<group id>:" + it.
inline constexpr const char* kInGroup = "tasks.";
} // namespace task_action

// `todoAction` (a todo_action id, with its item's id) on group `group`'s tasks.
std::string inGroup(std::uint64_t group, const std::string& todoAction);

struct TaskGroup {
    std::uint64_t id = 0; // stable while the app runs: its actions name it
    std::string name;
    TodoList tasks;
};

// A group deleted, kept for its Undo: where it was, and the empty group that
// took its place if it was the last one (it goes again on Undo, if still empty).
struct RemovedGroup {
    TaskGroup group;
    std::size_t index = 0;
    std::optional<std::uint64_t> standIn;
};

// Where a to-do action landed: its group, and the to-do it named (if any).
struct TaskTarget {
    std::uint64_t group = 0;
    std::string todoAction; // the action within the group, as TodoList::apply takes it
};

class TaskBoard {
public:
    TaskBoard(); // one empty group, "Tasks"

    const std::vector<TaskGroup>& groups() const { return groups_; }
    const TaskGroup* group(std::uint64_t id) const;

    std::uint64_t addGroup(); // at the end, named after the ones there ("Group 2"); its id
    bool renameGroup(std::uint64_t id, const std::string& text); // its first non-blank line, capped; blank changes nothing
    // Gone, and returned for an Undo. The last one: a new empty "Tasks" in its place.
    std::optional<RemovedGroup> removeGroup(std::uint64_t id);
    void restoreGroup(const RemovedGroup& removed); // back where it was

    // A group.* action (new, rename) or a to-do action in a group (inGroup).
    // True if it changed the board. Deleting a group is the provider's: it keeps it for its Undo.
    bool apply(const std::string& actionId, const std::string& text, float value = 0);
    // A to-do action's group and the action within it; none if it isn't one, or the group is gone.
    std::optional<TaskTarget> targetOf(const std::string& actionId) const;
    TodoList* tasksOf(std::uint64_t id);

    // "# <name>" over each group's TodoList::save. Lines before any "# " (a
    // list saved before groups) are a "Tasks" group's.
    std::string save() const;
    static TaskBoard load(const std::string& saved);

private:
    std::vector<TaskGroup> groups_;
    std::uint64_t nextId_ = 1;
};

} // namespace cup
