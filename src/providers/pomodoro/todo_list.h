#pragma once
// Pomodoro's to-do list (decisions 016, 019): what to work on this session. Pure:
// the list, the actions on it, how it's saved, and the rows it adds to the
// card. Pomodoro owns one and shows it under the timer.

#include "core/content.h"
#include "core/island.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cup {

namespace todo_action {
inline constexpr const char* kAdd = "todo.add";         // the field's action: Enter, or Ctrl+V into it
inline constexpr const char* kToggle = "todo.toggle.";  // + the item's id
inline constexpr const char* kDelete = "todo.delete.";  // + the item's id: at once, no question
inline constexpr const char* kScroll = "todo.scroll";   // the wheel over the list: Action::value = steps
// The clear buttons only ask (a dialog); its confirm button sends the "...Confirmed" one.
inline constexpr const char* kClearDone = "todo.clearDone";
inline constexpr const char* kClearAll = "todo.clearAll";
inline constexpr const char* kClearDoneConfirmed = "todo.clearDone.confirmed";
inline constexpr const char* kClearAllConfirmed = "todo.clearAll.confirmed";
} // namespace todo_action

struct Todo {
    std::uint64_t id = 0; // stable while the app runs: the checkbox's action names it
    std::string text;
    bool done = false;
    bool operator==(const Todo&) const = default;
};

class TodoList {
public:
    const std::vector<Todo>& items() const { return items_; }

    // Every non-blank line becomes a to-do, trimmed (a paste of several lines adds several).
    bool add(const std::string& text);
    bool toggle(std::uint64_t id);
    bool remove(std::uint64_t id);
    bool clearDone();
    bool clearAll();
    // A todo.* action that changes the list (add, toggle, delete, a confirmed
    // clear). True if it did. The clear buttons' own actions only ask: see clearQuestion.
    bool apply(const std::string& actionId, const std::string& text);

    // One per line: "[x] text" or "[ ] text". Unknown lines are read as open to-dos.
    std::string save() const;
    static TodoList load(const std::string& saved);

private:
    std::vector<Todo> items_;
    std::uint64_t nextId_ = 1;
};

// The question a clear button asks (kClearDone / kClearAll), over `card`.
Dialog clearQuestion(const std::string& actionId, const TodoList& list, const std::string& card);

// Where the list's window can start: 0 .. items - todoVisibleItems.
std::size_t clampScroll(long long first, const TodoList& list);

// The list's rows for a Centred card: the field (Ctrl+V pastes), a window
// of todoVisibleItems to-dos from `first` (checkbox + text, struck when
// done, a click anywhere on the row ticks it, a delete button shown on
// hover; the wheel scrolls), what's above and below it, then Clear solved /
// Clear all while there's anything to clear.
void appendTodoRows(Slot& slot, const TodoList& list, std::size_t first = 0);

} // namespace cup
