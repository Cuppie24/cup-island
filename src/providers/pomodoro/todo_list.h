#pragma once
// Pomodoro's to-do list (decision 015): what to work on this session. Pure:
// the list, the actions on it, how it's saved, and the rows it adds to the
// card. Pomodoro owns one and shows it under the timer.

#include "core/content.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cup {

namespace todo_action {
inline constexpr const char* kAdd = "todo.add";         // the field's action: Enter, or Ctrl+V into it
inline constexpr const char* kPaste = "todo.paste";     // the paste button: the provider reads the clipboard
inline constexpr const char* kToggle = "todo.toggle.";  // + the item's id
inline constexpr const char* kClearDone = "todo.clearDone";
inline constexpr const char* kClearAll = "todo.clearAll";
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
    bool clearDone();
    bool clearAll();
    // A todo.* action (not paste: that needs the clipboard). True if the list changed.
    bool apply(const std::string& actionId, const std::string& text);

    // One per line: "[x] text" or "[ ] text". Unknown lines are read as open to-dos.
    std::string save() const;
    static TodoList load(const std::string& saved);

private:
    std::vector<Todo> items_;
    std::uint64_t nextId_ = 1;
};

// The list's rows for a Centred card: the field and paste button, the first
// todoVisibleItems to-dos (checkbox + text, struck when done), "+n more",
// then Clear solved / Clear all while there's anything to clear.
void appendTodoRows(Slot& slot, const TodoList& list);

} // namespace cup
