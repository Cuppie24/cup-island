#pragma once
// Pomodoro's to-do list (decisions 016, 020): what to work on this session. Pure:
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
inline constexpr const char* kEdit = "todo.edit.";      // + the item's id: Action::text = its new text (edited in place)
inline constexpr const char* kMove = "todo.move.";      // + the item's id: Action::value = its new index (dragged)
inline constexpr const char* kList = "todos";           // the card's list (Row::list): scrolled by the host
inline constexpr const char* kShowDone = "todo.done.show"; // the Done row: shows or hides the solved ones (decision 071)
inline constexpr const char* kUndoDelete = "todo.undo";    // the delete toast's Undo: the last deleted one back (decision 071)
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
    // Its place in the whole list, solved or not: unticked, it goes back
    // there among the open ones (decision 071). A drag gives it a new one.
    double rank = 0;
    bool operator==(const Todo&) const = default;
};

class TodoList {
public:
    // Open to-dos first, in their order (by rank), then solved ones, in the order they were solved.
    const std::vector<Todo>& items() const { return items_; }
    // The solved ones are shown under the Done row (decision 071): hidden until it's clicked, each run.
    bool doneShown() const { return doneShown_; }

    // Every non-blank line becomes a to-do, trimmed (a paste of several lines adds several).
    bool add(const std::string& text);
    bool toggle(std::uint64_t id);
    bool remove(std::uint64_t id);
    bool rename(std::uint64_t id, const std::string& text); // its first non-blank line; blank changes nothing
    bool move(std::uint64_t id, std::size_t index);       // to `index`, kept within its group (open or solved)
    bool clearDone();
    bool clearAll();
    void restore(const Todo& todo); // a deleted one back (its toast's Undo, decision 071), in its place
    // A todo.* action that changes the list (add, toggle, delete, edit, move,
    // a confirmed clear, the Done row). True if it did. The clear buttons' own actions only ask: see clearQuestion.
    // A move's index counts the list's rows, the Done row among them.
    bool apply(const std::string& actionId, const std::string& text, float value = 0);

    // One per line: "[x] text" or "[ ] text", in rank order, so an unticked one
    // finds its place after a restart too. Unknown lines are read as open to-dos.
    std::string save() const;
    static TodoList load(const std::string& saved);

private:
    void group(); // open ones first, by rank; solved ones after, their order kept
    std::vector<Todo> items_;
    std::uint64_t nextId_ = 1;
    double nextRank_ = 0;
    bool doneShown_ = false;
};

// The question a clear button asks (kClearDone / kClearAll), over `card`.
Dialog clearQuestion(const std::string& actionId, const TodoList& list, const std::string& card);

// The list's rows for a Centred card: the field (Ctrl+V pastes), then the
// to-dos as one host-scrolled list (decision 029): checkbox + text, struck
// when done; the checkbox ticks it (its click area reaches the row's start),
// a click anywhere else on it edits its text in place (decision 043); a
// delete button on hover; drag to reorder. Then Clear solved / Clear all while there's
// anything to clear.
void appendTodoRows(Slot& slot, const TodoList& list);

} // namespace cup
