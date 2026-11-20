// Pomodoro's tasks in its window (decisions 016, 071, 117): groups and the
// tasks in them, a delete's Undo row. Split out of pomodoro_provider.cpp,
// which runs the timer.

#include "providers/pomodoro/pomodoro_provider.h"

#include "core/tokens.h"

#include <algorithm>
#include <charconv>

namespace cup {
namespace {

bool startsWith(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }

// The to-do a "<prefix><id>" action names in `list`, if it's that action.
const Todo* named(const TodoList& list, const std::string& action, const char* prefix)
{
    if (!startsWith(action, prefix)) return nullptr;
    std::uint64_t id = 0;
    const char* first = action.c_str() + std::char_traits<char>::length(prefix);
    if (std::from_chars(first, action.c_str() + action.size(), id).ec != std::errc{}) return nullptr;
    auto it = std::find_if(list.items().begin(), list.items().end(), [&](const Todo& t) { return t.id == id; });
    return it == list.items().end() ? nullptr : &*it;
}

} // namespace

bool PomodoroProvider::taskAction(const Action& action)
{
    namespace task = task_action;
    namespace todo = todo_action;
    const std::string& id = action.actionId;
    auto changed = [&] {
        host_->save(kTodosKey, tasks_.save());
        showWindow();
    };

    // Undo (decision 071): the last task or group deleted, back where it was.
    if (id == kPomodoroUndo) {
        if (deleted_) {
            if (auto* t = std::get_if<DeletedTask>(&*deleted_)) {
                if (TodoList* list = tasks_.tasksOf(t->group)) list->restore(t->task);
            } else {
                tasks_.restoreGroup(std::get<RemovedGroup>(*deleted_));
            }
        }
        deleted_.reset();
        changed();
        return true;
    }
    // A group with tasks asks first, in a question over the window (decision 122); an empty one, or
    // one confirmed, goes at once. An empty one's Undo is a toast; a confirmed one needs none.
    bool confirmed = startsWith(id, task::kDeleteGroupConfirmed);
    if (confirmed || startsWith(id, task::kDeleteGroup)) {
        std::uint64_t group = 0;
        const char* first = id.c_str() + std::char_traits<char>::length(confirmed ? task::kDeleteGroupConfirmed : task::kDeleteGroup);
        std::from_chars(first, id.c_str() + id.size(), group);
        const TaskGroup* g = tasks_.group(group);
        if (!g) return true;
        std::size_t count = g->tasks.items().size();
        if (!confirmed && count > 0) {
            std::string tasks = std::to_string(count) + (count == 1 ? " task goes" : " tasks go");
            host_->ask({ .title = "Delete \u201C" + g->name + "\u201D?", .message = "Its " + tasks + " with it.", .confirmLabel = "Delete",
                         .confirmAction = task::kDeleteGroupConfirmed + std::to_string(group), .destructive = true, .windowId = kWindowId });
            return true;
        }
        if (auto gone = tasks_.removeGroup(group)) {
            deleted_ = std::move(*gone);
            if (!confirmed) offerUndo();
        }
        changed();
        return true;
    }

    // Its menu's Rename (decision 142): its name in a question over the window; confirmed, kRenameGroup with the text.
    if (startsWith(id, task::kAskGroupName)) {
        std::uint64_t group = 0;
        const char* first = id.c_str() + std::char_traits<char>::length(task::kAskGroupName);
        std::from_chars(first, id.c_str() + id.size(), group);
        if (const TaskGroup* g = tasks_.group(group)) askName(*g, false);
        return true;
    }

    // Dragged (decision 129): a group among the groups; a task within its group or into another.
    if (startsWith(id, task::kMoveGroup)) {
        std::uint64_t group = 0;
        const char* first = id.c_str() + std::char_traits<char>::length(task::kMoveGroup);
        std::from_chars(first, id.c_str() + id.size(), group);
        if (tasks_.moveGroup(group, static_cast<std::size_t>(std::max(0.0f, action.value)))) changed();
        return true;
    }
    if (startsWith(id, task::kMoveTask)) {
        std::uint64_t from = 0, task = 0, to = 0;
        const char* first = id.c_str() + std::char_traits<char>::length(task::kMoveTask);
        const char* end = id.c_str() + id.size();
        auto [dot, error] = std::from_chars(first, end, from);
        if (error == std::errc{} && dot < end && *dot == '.') std::from_chars(dot + 1, end, task);
        std::string into = action.text.rfind("group-", 0) == 0 ? action.text.substr(6) : "";
        std::from_chars(into.data(), into.data() + into.size(), to);
        if (tasks_.moveTask(from, task, to, static_cast<std::size_t>(std::max(0.0f, action.value)))) {
            deleted_.reset();
            changed();
        }
        return true;
    }

    // "Add task" opens a row to write one in, under it (decision 132); left, it's added and goes, or
    // just goes if it was left blank. Another group's open row was left first, by the click.
    bool written = startsWith(id, task::kTaskWritten);
    if (written || startsWith(id, task::kWriteTask)) {
        std::uint64_t group = 0;
        const char* first = id.c_str() + std::char_traits<char>::length(written ? task::kTaskWritten : task::kWriteTask);
        std::from_chars(first, id.c_str() + id.size(), group);
        if (!written) {
            if (tasks_.group(group)) writingIn_ = group;
        } else if (writingIn_ == group) {
            writingIn_.reset();
        }
        if (written && tasks_.apply(inGroup(group, todo::kAdd), action.text)) { // blank: nothing added
            deleted_.reset();
            host_->save(kTodosKey, tasks_.save());
        }
        showWindow();
        return true;
    }

    auto target = tasks_.targetOf(id);
    if (!startsWith(id, task::kNewGroup) && !startsWith(id, task::kRenameGroup) && !target) return startsWith(id, task::kInGroup);
    // A group's menu's Delete solved / all tasks (decision 161): asked over the window; confirmed, the
    // to-do list's own confirmed clear, in that group. Gone for good, as a confirmed group delete is.
    if (target && (target->todoAction == todo::kClearDone || target->todoAction == todo::kClearAll)) {
        if (const TaskGroup* g = tasks_.group(target->group)) host_->ask(clearGroupQuestion(*g, target->todoAction == todo::kClearDone));
        return true;
    }
    std::optional<DeletedTask> deleting;
    if (target)
        if (const Todo* t = named(*tasks_.tasksOf(target->group), target->todoAction, todo::kDelete)) deleting = DeletedTask{ target->group, *t };
    if (!tasks_.apply(id, action.text, action.value)) return true;
    if (deleting) {
        deleted_ = *deleting;
        offerUndo();
    } else {
        deleted_.reset(); // the next change: there's nothing to undo any more
    }
    if (target && startsWith(target->todoAction, todo::kToggle)) countTask(*target);
    changed();
    if (id == task::kNewGroup) askName(tasks_.groups().back(), true); // its name asked at once, "Group 2" typed over (decision 142)
    return true;
}

void PomodoroProvider::countTask(const TaskTarget& target)
{
    const TodoList* list = tasks_.tasksOf(target.group);
    const Todo* t = list ? named(*list, target.todoAction, todo_action::kToggle) : nullptr;
    if (!t) return;
    int& done = today().todosDone;
    done = t->done ? done + 1 : std::max(0, done - 1); // unticked: taken back, never below none
    saveStats();
}

// A group's name asked in a question over the window (decision 142): its field starts with the name, all
// of it selected. Confirmed, kRenameGroup with what's in it (blank: unchanged); cancelled, it stays.
void PomodoroProvider::askName(const TaskGroup& g, bool made)
{
    host_->ask({ .title = made ? std::string("Name the new group") : std::string("Rename group"), .confirmLabel = made ? std::string("Save") : std::string("Rename"),
                 .confirmAction = task_action::kRenameGroup + std::to_string(g.id), .windowId = kWindowId, .field = g.name,
                 .fieldMaxLength = tokens().taskGroupNameMaxLength });
}

// Deleting a group's solved or all tasks, asked over the window (decision 161): its name and how many go.
Dialog PomodoroProvider::clearGroupQuestion(const TaskGroup& g, bool solved) const
{
    const auto& items = g.tasks.items();
    std::size_t count = solved ? static_cast<std::size_t>(std::count_if(items.begin(), items.end(), [](const Todo& t) { return t.done; })) : items.size();
    std::string tasks = std::to_string(count) + (solved ? " solved" : "") + (count == 1 ? " task" : " tasks");
    return { .title = (solved ? "Delete solved tasks in \u201C" : "Delete all tasks in \u201C") + g.name + "\u201D?",
             .message = (count == 1 ? tasks + " will be deleted." : (solved ? "" : "All ") + tasks + " will be deleted."),
             .confirmLabel = "Delete",
             .confirmAction = inGroup(g.id, solved ? todo_action::kClearDoneConfirmed : todo_action::kClearAllConfirmed),
             .destructive = true,
             .windowId = kWindowId };
}

// The host's toast over the window (decision 121), as a card's delete has (decision 071).
void PomodoroProvider::offerUndo()
{
    if (!deleted_) return;
    std::string text = std::holds_alternative<DeletedTask>(*deleted_) ? "Task deleted"
                                                                      : "“" + std::get<RemovedGroup>(*deleted_).group.name + "” deleted";
    host_->toast({ .text = text, .actionLabel = "Undo", .actionId = kPomodoroUndo, .windowId = kWindowId });
}

} // namespace cup
