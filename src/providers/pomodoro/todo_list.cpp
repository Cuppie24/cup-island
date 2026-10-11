#include "providers/pomodoro/todo_list.h"

#include "core/tokens.h"

#include <algorithm>
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
    if (actionId == act::kClearDone) return clearDone();
    if (actionId == act::kClearAll) return clearAll();
    std::string prefix = act::kToggle;
    if (actionId.rfind(prefix, 0) == 0) {
        try {
            return toggle(std::stoull(actionId.substr(prefix.size())));
        } catch (...) {
            return false; // not a number: not ours
        }
    }
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

void appendTodoRows(Slot& slot, const TodoList& list)
{
    const auto& items = list.items();
    slot.push_back(Row{ .items = { TextInput{ .actionId = act::kAdd, .placeholder = "Add a to-do…" } },
                        .trailing = { Button{ .glyph = Glyph::Paste, .actionId = act::kPaste, .compact = true } } });

    std::size_t shown = std::min(items.size(), tokens().todoVisibleItems);
    for (std::size_t i = 0; i < shown; ++i) {
        const Todo& t = items[i];
        Button box{ .glyph = t.done ? Glyph::Checked : Glyph::Unchecked, .actionId = act::kToggle + std::to_string(t.id), .compact = true };
        Text text{ .value = t.text, .style = t.done ? TextStyle::Caption : TextStyle::Body, .struck = t.done };
        slot.push_back(row({ box, text }));
    }
    if (items.size() > shown)
        slot.push_back(row({ Text{ .value = "+" + std::to_string(items.size() - shown) + " more", .style = TextStyle::Caption, .centred = true } }));

    if (items.empty()) return;
    bool anyDone = std::any_of(items.begin(), items.end(), [](const Todo& t) { return t.done; });
    slot.push_back(Row{ .items = { Button{ .actionId = act::kClearDone, .enabled = anyDone, .label = "Clear solved", .compact = true },
                                   Button{ .actionId = act::kClearAll, .label = "Clear all", .compact = true } },
                        .align = Align::Center });
}

} // namespace cup
