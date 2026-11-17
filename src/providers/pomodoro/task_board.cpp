#include "providers/pomodoro/task_board.h"

#include "core/text_edit.h"
#include "core/tokens.h"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace cup {
namespace {

namespace act = task_action;

constexpr const char* kFirstName = "Tasks";

std::string trim(const std::string& s)
{
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

// A name: its first non-blank line, at most taskGroupNameMaxLength characters. "": none.
std::string nameFrom(const std::string& text)
{
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);)
        if (std::string name = trim(line); !name.empty()) return truncateChars(name, tokens().taskGroupNameMaxLength);
    return "";
}

std::optional<std::uint64_t> number(std::string_view text)
{
    std::uint64_t n = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), n);
    if (error != std::errc{} || end != text.data() + text.size() || text.empty()) return std::nullopt;
    return n;
}

std::optional<std::uint64_t> idAfter(const std::string& actionId, const char* prefix)
{
    std::string_view p = prefix;
    if (actionId.rfind(p, 0) != 0) return std::nullopt;
    return number(std::string_view(actionId).substr(p.size()));
}

} // namespace

std::string inGroup(std::uint64_t group, const std::string& todoAction) { return act::kInGroup + std::to_string(group) + ":" + todoAction; }

TaskBoard::TaskBoard() { groups_.push_back({ nextId_++, kFirstName, {} }); }

const TaskGroup* TaskBoard::group(std::uint64_t id) const
{
    auto it = std::find_if(groups_.begin(), groups_.end(), [&](const TaskGroup& g) { return g.id == id; });
    return it == groups_.end() ? nullptr : &*it;
}

TodoList* TaskBoard::tasksOf(std::uint64_t id)
{
    auto it = std::find_if(groups_.begin(), groups_.end(), [&](const TaskGroup& g) { return g.id == id; });
    return it == groups_.end() ? nullptr : &it->tasks;
}

std::uint64_t TaskBoard::addGroup()
{
    auto taken = [&](const std::string& name) { return std::any_of(groups_.begin(), groups_.end(), [&](const TaskGroup& g) { return g.name == name; }); };
    std::string name;
    for (std::size_t n = groups_.size() + 1; name.empty() || taken(name); ++n) name = "Group " + std::to_string(n);
    groups_.push_back({ nextId_, name, {} });
    return nextId_++;
}

bool TaskBoard::renameGroup(std::uint64_t id, const std::string& text)
{
    std::string name = nameFrom(text);
    auto it = std::find_if(groups_.begin(), groups_.end(), [&](const TaskGroup& g) { return g.id == id; });
    if (name.empty() || it == groups_.end() || it->name == name) return false;
    it->name = name;
    return true;
}

std::optional<RemovedGroup> TaskBoard::removeGroup(std::uint64_t id)
{
    auto it = std::find_if(groups_.begin(), groups_.end(), [&](const TaskGroup& g) { return g.id == id; });
    if (it == groups_.end()) return std::nullopt;
    RemovedGroup gone{ std::move(*it), static_cast<std::size_t>(it - groups_.begin()), std::nullopt };
    groups_.erase(it);
    if (groups_.empty()) { // never none: somewhere to add the next task
        gone.standIn = nextId_;
        groups_.push_back({ nextId_++, kFirstName, {} });
    }
    return gone;
}

void TaskBoard::restoreGroup(const RemovedGroup& removed)
{
    if (group(removed.group.id)) return;
    if (removed.standIn) // the empty group that stood in for it goes again, unless it got tasks
        std::erase_if(groups_, [&](const TaskGroup& g) { return g.id == *removed.standIn && g.tasks.items().empty(); });
    groups_.insert(groups_.begin() + static_cast<std::ptrdiff_t>(std::min(removed.index, groups_.size())), removed.group);
}

bool TaskBoard::moveGroup(std::uint64_t id, std::size_t index)
{
    auto it = std::find_if(groups_.begin(), groups_.end(), [&](const TaskGroup& g) { return g.id == id; });
    if (it == groups_.end()) return false;
    std::size_t from = static_cast<std::size_t>(it - groups_.begin()), to = std::min(index, groups_.size() - 1);
    if (from == to) return false;
    TaskGroup moving = std::move(*it);
    groups_.erase(it);
    groups_.insert(groups_.begin() + static_cast<std::ptrdiff_t>(to), std::move(moving));
    return true;
}

bool TaskBoard::moveTask(std::uint64_t from, std::uint64_t task, std::uint64_t to, std::size_t index)
{
    TodoList* source = tasksOf(from);
    TodoList* target = tasksOf(to);
    if (!source || !target) return false;
    std::optional<Todo> taken = source->take(task);
    if (!taken) return false;
    target->place(*taken, index);
    return true;
}

std::optional<TaskTarget> TaskBoard::targetOf(const std::string& actionId) const
{
    std::string_view p = act::kInGroup;
    if (actionId.rfind(p, 0) != 0) return std::nullopt;
    auto colon = actionId.find(':', p.size());
    if (colon == std::string::npos) return std::nullopt;
    auto id = number(std::string_view(actionId).substr(p.size(), colon - p.size()));
    if (!id || !group(*id)) return std::nullopt;
    return TaskTarget{ *id, actionId.substr(colon + 1) };
}

bool TaskBoard::apply(const std::string& actionId, const std::string& text, float value)
{
    if (actionId == act::kNewGroup) return addGroup() != 0;
    if (auto id = idAfter(actionId, act::kRenameGroup)) return renameGroup(*id, text);
    auto target = targetOf(actionId);
    if (!target) return false;
    return tasksOf(target->group)->apply(target->todoAction, text, value);
}

std::string TaskBoard::save() const
{
    std::string out;
    for (const TaskGroup& g : groups_) out += "# " + g.name + "\n" + g.tasks.save();
    return out;
}

TaskBoard TaskBoard::load(const std::string& saved)
{
    // Split at the "# " lines; each part is a TodoList's own save.
    std::vector<std::pair<std::string, std::string>> parts; // name, its lines
    std::istringstream lines(saved);
    for (std::string line; std::getline(lines, line);) {
        if (line.rfind("# ", 0) == 0) {
            std::string name = nameFrom(line.substr(2));
            parts.push_back({ name.empty() ? kFirstName : name, "" });
            continue;
        }
        if (parts.empty()) parts.push_back({ kFirstName, "" }); // saved before groups
        parts.back().second += line + "\n";
    }
    TaskBoard board;
    if (parts.empty()) return board;
    board.groups_.clear();
    for (auto& [name, list] : parts) board.groups_.push_back({ board.nextId_++, name, TodoList::load(list) });
    return board;
}

} // namespace cup
