#include "providers/pomodoro/todo_list.h"

#include "core/tokens.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>

namespace cup {
namespace {

namespace act = todo_action;

constexpr std::size_t kMaxLength = 200; // a to-do, not an essay: the card shows one line

std::string trim(const std::string& s)
{
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

} // namespace

bool TodoList::add(const std::string& text)
{
    bool added = false;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        line = trim(line);
        if (line.empty()) continue;
        if (line.size() > kMaxLength) line.resize(kMaxLength);
        items_.push_back({ nextId_++, line, false });
        added = true;
    }
    group();
    return added;
}

void TodoList::group()
{
    std::stable_partition(items_.begin(), items_.end(), [](const Todo& t) { return !t.done; });
}

bool TodoList::toggle(std::uint64_t id)
{
    for (Todo& t : items_) {
        if (t.id != id) continue;
        t.done = !t.done;
        group(); // ticked: down to the solved ones; unticked: back up with the open ones
        return true;
    }
    return false;
}

bool TodoList::remove(std::uint64_t id)
{
    return std::erase_if(items_, [&](const Todo& t) { return t.id == id; }) > 0;
}

bool TodoList::rename(std::uint64_t id, const std::string& text)
{
    std::istringstream lines(text);
    std::string first;
    for (std::string line; first.empty() && std::getline(lines, line);) first = trim(line);
    if (first.empty()) return false;
    if (first.size() > kMaxLength) first.resize(kMaxLength);
    for (Todo& t : items_) {
        if (t.id != id || t.text == first) continue;
        t.text = first;
        return true;
    }
    return false;
}

bool TodoList::move(std::uint64_t id, std::size_t index)
{
    auto it = std::find_if(items_.begin(), items_.end(), [&](const Todo& t) { return t.id == id; });
    if (it == items_.end()) return false;
    // Its group's span: open ones first, then solved (group()).
    std::size_t open = static_cast<std::size_t>(std::count_if(items_.begin(), items_.end(), [](const Todo& t) { return !t.done; }));
    std::size_t lo = it->done ? open : 0, hi = it->done ? items_.size() - 1 : open - 1;
    std::size_t from = static_cast<std::size_t>(it - items_.begin()), to = std::clamp(index, lo, hi);
    if (from == to) return false;
    Todo moving = *it;
    items_.erase(it);
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(to), moving);
    return true;
}

bool TodoList::clearDone()
{
    return std::erase_if(items_, [](const Todo& t) { return t.done; }) > 0;
}

bool TodoList::clearAll()
{
    bool any = !items_.empty();
    items_.clear();
    return any;
}

bool TodoList::apply(const std::string& actionId, const std::string& text, float value)
{
    if (actionId == act::kAdd) return add(text);
    if (actionId == act::kClearDoneConfirmed) return clearDone();
    if (actionId == act::kClearAllConfirmed) return clearAll();
    // "<prefix><id>": the item it names, or nothing if that isn't a number.
    auto idAfter = [&](const char* prefix) -> std::optional<std::uint64_t> {
        std::string p = prefix;
        if (actionId.rfind(p, 0) != 0) return std::nullopt;
        try {
            return std::stoull(actionId.substr(p.size()));
        } catch (...) {
            return std::nullopt;
        }
    };
    if (auto id = idAfter(act::kToggle)) return toggle(*id);
    if (auto id = idAfter(act::kDelete)) return remove(*id);
    if (auto id = idAfter(act::kEdit)) return rename(*id, text);
    if (auto id = idAfter(act::kMove)) return value >= 0 && move(*id, static_cast<std::size_t>(std::lround(value)));
    return false;
}

std::string TodoList::save() const
{
    std::string out;
    for (const Todo& t : items_) out += (t.done ? "[x] " : "[ ] ") + t.text + "\n";
    return out;
}

TodoList TodoList::load(const std::string& saved)
{
    TodoList list;
    std::istringstream lines(saved);
    for (std::string line; std::getline(lines, line);) {
        bool done = line.rfind("[x] ", 0) == 0;
        if (done || line.rfind("[ ] ", 0) == 0) line = line.substr(4);
        line = trim(line);
        if (line.empty()) continue;
        if (line.size() > kMaxLength) line.resize(kMaxLength);
        list.items_.push_back({ list.nextId_++, line, done });
    }
    list.group(); // a list saved before grouping
    return list;
}

Dialog clearQuestion(const std::string& actionId, const TodoList& list, const std::string& card)
{
    const auto& items = list.items();
    auto plural = [](std::size_t n) { return std::to_string(n) + (n == 1 ? " task" : " tasks"); };
    if (actionId == act::kClearDone) {
        auto done = static_cast<std::size_t>(std::count_if(items.begin(), items.end(), [](const Todo& t) { return t.done; }));
        return { .activityId = card, .title = "Clear solved tasks?", .message = plural(done) + " will be removed.",
                 .confirmLabel = "Clear", .confirmAction = act::kClearDoneConfirmed, .destructive = true };
    }
    return { .activityId = card, .title = "Clear all tasks?", .message = "All " + plural(items.size()) + " will be removed.",
             .confirmLabel = "Clear all", .confirmAction = act::kClearAllConfirmed, .destructive = true };
}

void appendTodoRows(Slot& slot, const TodoList& list)
{
    const auto& items = list.items();
    slot.push_back(row({ TextInput{ .actionId = act::kAdd, .placeholder = "Add a to-do…" } }));

    for (const Todo& t : items) {
        std::string id = std::to_string(t.id);
        Button box{ .glyph = t.done ? Glyph::Checked : Glyph::Unchecked, .actionId = act::kToggle + id, .compact = true };
        Text text{ .value = t.text, .style = t.done ? TextStyle::Caption : TextStyle::Body, .struck = t.done, .editAction = act::kEdit + id };
        Button edit{ .glyph = Glyph::Edit, .actionId = act::kEdit + id, .compact = true, .revealOnHover = true, .editsText = true };
        Button remove{ .glyph = Glyph::Delete, .actionId = act::kDelete + id, .compact = true, .revealOnHover = true };
        slot.push_back(Row{ .items = { box, text }, .trailing = { edit, remove }, .clickAnywhere = true, .list = act::kList,
                            .moveAction = act::kMove + id, .sortGroup = t.done ? 1 : 0, .id = "todo-" + id, .listItem = true });
    }

    if (items.empty()) return;
    bool anyDone = std::any_of(items.begin(), items.end(), [](const Todo& t) { return t.done; });
    slot.push_back(Row{ .items = { Button{ .actionId = act::kClearDone, .enabled = anyDone, .label = "Clear solved", .compact = true },
                                   Button{ .actionId = act::kClearAll, .label = "Clear all", .compact = true } },
                        .align = Align::Center });
}
} // namespace cup
