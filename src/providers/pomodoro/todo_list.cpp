#include "providers/pomodoro/todo_list.h"

#include "core/tokens.h"

#include <algorithm>
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
    return added;
}

bool TodoList::toggle(std::uint64_t id)
{
    for (Todo& t : items_) {
        if (t.id != id) continue;
        t.done = !t.done;
        return true;
    }
    return false;
}

bool TodoList::remove(std::uint64_t id)
{
    return std::erase_if(items_, [&](const Todo& t) { return t.id == id; }) > 0;
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

bool TodoList::apply(const std::string& actionId, const std::string& text)
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
        std::size_t before = list.items_.size();
        list.add(line);
        if (list.items_.size() > before) list.items_.back().done = done;
    }
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

std::size_t clampScroll(long long first, const TodoList& list)
{
    long long last = static_cast<long long>(list.items().size()) - static_cast<long long>(tokens().todoVisibleItems);
    return static_cast<std::size_t>(std::clamp(first, 0LL, std::max(last, 0LL)));
}

void appendTodoRows(Slot& slot, const TodoList& list, std::size_t first)
{
    const auto& items = list.items();
    slot.push_back(row({ TextInput{ .actionId = act::kAdd, .placeholder = "Add a to-do…" } }));

    first = clampScroll(static_cast<long long>(first), list);
    std::size_t end = std::min(items.size(), first + tokens().todoVisibleItems);
    for (std::size_t i = first; i < end; ++i) {
        const Todo& t = items[i];
        std::string id = std::to_string(t.id);
        Button box{ .glyph = t.done ? Glyph::Checked : Glyph::Unchecked, .actionId = act::kToggle + id, .compact = true };
        Text text{ .value = t.text, .style = t.done ? TextStyle::Caption : TextStyle::Body, .struck = t.done };
        Button remove{ .glyph = Glyph::Delete, .actionId = act::kDelete + id, .compact = true, .revealOnHover = true };
        slot.push_back(Row{ .items = { box, text }, .trailing = { remove }, .clickAnywhere = true,
                              .scrollAction = act::kScroll, .listItem = true });
    }

    // What's out of the window, so it's clear the list scrolls (the wheel works here too).
    std::size_t above = first, below = items.size() - end;
    if (above + below > 0) {
        std::string hint = above && below ? std::to_string(above) + " above · " + std::to_string(below) + " below"
                         : below          ? std::to_string(below) + " more below"
                                          : std::to_string(above) + " more above";
        slot.push_back(Row{ .items = { Text{ .value = hint, .style = TextStyle::Caption, .centred = true } }, .scrollAction = act::kScroll });
    }

    if (items.empty()) return;
    bool anyDone = std::any_of(items.begin(), items.end(), [](const Todo& t) { return t.done; });
    slot.push_back(Row{ .items = { Button{ .actionId = act::kClearDone, .enabled = anyDone, .label = "Clear solved", .compact = true },
                                   Button{ .actionId = act::kClearAll, .label = "Clear all", .compact = true } },
                        .align = Align::Center });
}
} // namespace cup
